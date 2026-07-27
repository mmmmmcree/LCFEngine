# vkc DynamicRender 布局转换设计

> 把 attachment 的入口/出口 layout 转换收进 `DynamicRender::begin/end`，让静态与动态两条路径
> 共用同一份 `AttachmentSetInfo` 声明；同时为 `VK_KHR_unified_image_layouts` 预留策略点。
>
> 前置文档：`vkc-dynamic-render-impl.md`（动态路径落地）、
> `vkc-attachment-set-authority-design.md`（attachment set 归属）。
> 现状基线：005 示例手写了 28 行 barrier（`005_hello_static_pipeline_main.cpp:289-318`）。

## 1. 目标与范围

RenderPass 路径的 layout 转换是隐式的：`AttachmentDescription2::initialLayout` /
`finalLayout` 加上驱动补的 external subpass dependency，调用方一行不写。动态路径没有这个
机制，005 里两条 `pipelineBarrier2` 只能手写。结果是同一个 attachment set 声明，换条路径要
多写 28 行——抽象没补齐，不是本质差异。

本文档要做的：

- `AttachmentSetInfo` 上的入口/出口声明由两条路径共用，example 代码在两条路径下逐字相同。
- `DynamicRender::begin/end` 各发一次批量 `pipelineBarrier2`，每帧零分配。
- 声明的单位是**意图**（下一个消费者是谁），不是 `vk::ImageLayout`——这是 mask 可推导和
  unified layouts 可开关的共同前提，理由见 §3。
- `ImageLayoutPolicy` 作为唯一的 layout 落地点，unified layouts 变成一个开关。

本文档**不**做：跨 pass 的 barrier 合并、subresource 范围收窄、静态路径的显式 external
dependency 生成。这些需要全局视野，属于 RenderGraph 层，见 §10。

## 2. 为什么只复用 initialLayout / finalLayout 不够

`initialLayout` / `finalLayout` 只定了 `vk::ImageMemoryBarrier2` 的两个字段。剩下四个
（`srcStageMask` / `srcAccessMask` / `dstStageMask` / `dstAccessMask`）在 RenderPass 路径里
由 external subpass dependency 提供——005 根本没写这条 dependency，驱动用了隐式默认
（`srcStage = eAllCommands`、`srcAccess = 0`），所以能跑。动态路径没有这个兜底。

两个方向的可推导性不对称：

- **出口**：`finalLayout` 基本等于「下一个消费者是谁」，可推。
- **入口**：`srcStageMask` / `srcAccessMask` 取决于**上一次谁碰过这张图**，是 pass 之间的
  信息，`DynamicRender` 看不到。

所以入口方向必须有一个保守默认（`eAllCommands` + `eNone`，即 005 手写的那个值）加一个
override 出口。这是这一层唯一一处明知在过度同步的地方，应当在 API 上留痕，而不是藏起来。

## 3. 为什么声明意图而不是 layout

第一版方案是让 `DynamicRenderInfo` 复用 `setInitialFinalLayout`，从 layout 反推 mask。这条路
在 unified layouts 面前会立刻断掉，两个原因：

**layout → mask 本身就有歧义。** `eShaderReadOnlyOptimal` 不能确定是 fragment、compute
还是 vertex 在读；`eTransferSrcOptimal` 不能区分 `eCopy` 与 `eResolve`。

**开了 unified layouts 之后所有 layout 都是 `eGeneral`，推导信息归零。** 这不是精度下降，
是彻底失效——`eGeneral → (eAllCommands, eMemoryRead|eMemoryWrite)` 是唯一安全的推导，等于
每个 barrier 都退化成全屏障，比手写还慢。

意图声明反过来同时喂两边：

```
AttachmentUsage --> ImageLayoutPolicy --> vk::ImageLayout   (unified 时统一成 eGeneral)
              \--> access_scope_of()  --> (stage mask, access mask)   (与 layout 无关)
```

mask 从意图直接推，不经过 layout，所以 unified layouts 开关不影响同步正确性——它只改
layout 字段。这是把意图作为声明单位的核心理由。

`vk::ImageLayout` 保留为 escape hatch（§4.3），供意图枚举没覆盖的场合使用；走这条路时
mask 必须显式给，因为推不出来。

### 3.1 `AttachmentUsage`

新增 `pipeline/graphics/enums.h`：

```cpp
enum class AttachmentUsage : uint8_t
{
    eNone,                  //- no transition, no barrier
    eDiscard,               //- contents are not preserved; resolves to eUndefined
    eColorAttachment,
    eDepthStencilAttachment,
    eDepthStencilReadOnly,
    eTransferSource,
    eTransferDestination,
    eShaderRead,
    ePresent,
};
```

`eNone` 与 `eDiscard` 的区别是本设计的两个 sentinel，语义要分清：

- `eDiscard`：内容可丢弃，落到 `eUndefined`。**会发 barrier**（必须从 `eUndefined` 转出）。
- `eNone`：不声明，**不发 barrier**。出口侧的默认值，表示「转换由调用方自己管」。

入口侧不提供 `eNone`——入口默认由 `loadOp` 推（§4.2），拿不到「什么都不做」这个语义，
需要的话把入口意图设成与 in-pass 意图相同即可。

### 3.2 mask 推导表

`utils/access_scope_utils.h`，free function，`snake_case`：

```cpp
struct AccessScope
{
    vk::PipelineStageFlags2 stage_mask;
    vk::AccessFlags2 access_mask;
};

constexpr AccessScope access_scope_of(AttachmentUsage usage) noexcept;
```

| `AttachmentUsage` | stage mask | access mask |
|---|---|---|
| `eNone` | `eNone` | `eNone` |
| `eDiscard` | `eAllCommands` | `eNone` |
| `eColorAttachment` | `eColorAttachmentOutput` | `eColorAttachmentWrite \| eColorAttachmentRead` |
| `eDepthStencilAttachment` | `eEarlyFragmentTests \| eLateFragmentTests` | `eDepthStencilAttachmentWrite \| eDepthStencilAttachmentRead` |
| `eDepthStencilReadOnly` | `eEarlyFragmentTests \| eLateFragmentTests \| eFragmentShader` | `eDepthStencilAttachmentRead` |
| `eTransferSource` | `eAllTransfer` | `eTransferRead` |
| `eTransferDestination` | `eAllTransfer` | `eTransferWrite` |
| `eShaderRead` | `eAllGraphics \| eComputeShader` | `eShaderSampledRead` |
| `ePresent` | `eNone` | `eNone` |

三处保守，都可以 override：

- `eTransferSource/Destination` 用 `eAllTransfer` 而不是 `eCopy`/`eBlit`/`eResolve`——005 走
  blit，写 `eBlit` 更准，但意图枚举不区分 transfer 子类。
- `eShaderRead` 的 stage 是并集，不知道哪个 stage 在采样。
- `ePresent` 全空：present 的同步靠 semaphore，barrier 只需要 layout 转换。

`eDiscard` 的 `eAllCommands` 是 §2 说的那个保守入口默认——它出现在这里是因为「丢弃」意味着
「不关心之前是谁写的」，等待所有前序命令是唯一安全解。

## 4. API 变更

### 4.1 `ImageLayoutPolicy`（新增 `utils/ImageLayoutPolicy.h`）

```cpp
class ImageLayoutPolicy
{
    using Self = ImageLayoutPolicy;
public:
    ~ImageLayoutPolicy() noexcept = default;
    ImageLayoutPolicy() noexcept = default;
    ImageLayoutPolicy(const Self &) noexcept = default;
    Self & operator=(const Self &) noexcept = default;
public:
    Self & enableUnifiedLayouts() noexcept { m_unified_enabled = true; return *this; }
    vk::ImageLayout resolve(AttachmentUsage usage) const noexcept;
    bool isUnifiedEnabled() const noexcept { return m_unified_enabled; }
private:
    bool m_unified_enabled = false;
};
```

`resolve()` 的映射：

| `AttachmentUsage` | 默认 | unified 开启后 |
|---|---|---|
| `eNone` | `eUndefined` | `eUndefined` |
| `eDiscard` | `eUndefined` | `eUndefined` |
| `eColorAttachment` | `eColorAttachmentOptimal` | `eGeneral` |
| `eDepthStencilAttachment` | `eDepthStencilAttachmentOptimal` | `eGeneral` |
| `eDepthStencilReadOnly` | `eDepthStencilReadOnlyOptimal` | `eGeneral` |
| `eTransferSource` | `eTransferSrcOptimal` | `eGeneral` |
| `eTransferDestination` | `eTransferDstOptimal` | `eGeneral` |
| `eShaderRead` | `eShaderReadOnlyOptimal` | `eGeneral` |
| `ePresent` | `ePresentSrcKHR` | `ePresentSrcKHR` |

两处不受 unified 影响，都不是可选的：`eUndefined` 承载「丢弃内容 + 初始化 metadata」语义，
规范明确要求保留并鼓励继续使用；`ePresentSrcKHR` 不在扩展覆盖范围内（compositor 在 Vulkan
之外）。见 §7。

`DeviceContext` 持有一份，创建时根据实际拿到的 feature 置位：

```cpp
const ImageLayoutPolicy & getImageLayoutPolicy() const noexcept { return m_layout_policy; }
```

### 4.2 `AttachmentSetInfo` / builder

`AttachmentSetInfoBuilder::build()` 增加 policy 参数，`AttachmentSetInfo` 存一份指针：

```cpp
AttachmentSetInfo build(const ImageLayoutPolicy & policy) const noexcept;
```

`AttachmentDescriptionInfo` 增加意图版 setter，与现有的 layout 版并存：

```cpp
Self & setInitialFinalUsage(
    const ImageLayoutPolicy & policy,
    AttachmentUsage entry,
    AttachmentUsage exit) noexcept
{
    m_description.root()
        .setInitialLayout(policy.resolve(entry))
        .setFinalLayout(policy.resolve(exit));
    return *this;
}
```

`AttachmentSetInfo` 新增两个 slot-indexed 数组存意图本身（layout 落地后信息就丢了，
mask 推导还要用）：

```cpp
private:
    const ImageLayoutPolicy * m_policy_p = nullptr;
    std::vector<AttachmentUsage> m_entry_usages;   //- slot indexed, defaults from loadOp
    std::vector<AttachmentUsage> m_exit_usages;    //- slot indexed, defaults to eNone
```

入口意图的默认值由 `loadOp` 决定，在 `setLoadStoreOp` 里同步更新（未被显式声明覆盖时）：

| `loadOp` | 默认入口意图 |
|---|---|
| `eClear` / `eDontCare` | `eDiscard` |
| `eLoad` | 与 in-pass 意图相同（layout 相等，只留 memory dependency） |

这条自动化掉了一类静默 bug：`loadOp = eLoad` 时 `initialLayout` 写成 `eUndefined` 会丢数据，
validation layer 不报。现在这个组合写不出来。

出口意图默认 `eNone`——不声明就不发出口 barrier，保持「不替你偷偷同步」。

### 4.3 `DynamicRenderInfo` 新增接口

```cpp
    //- entry defaults from loadOp; exit defaults to eNone (no barrier emitted)
    Self & setEntryUsage(details::attachment_key_c auto key, AttachmentUsage usage) noexcept;
    Self & setExitUsage(details::attachment_key_c auto key, AttachmentUsage usage) noexcept;
    Self & setInPassUsage(details::attachment_key_c auto key, AttachmentUsage usage) noexcept;
    //- overrides the derived scope; needed when the usage enum is too coarse, and
    //- mandatory when setLayout is used instead of setEntryUsage/setExitUsage
    Self & setEntrySrcScope(details::attachment_key_c auto key, const AccessScope & scope) noexcept;
    Self & setExitDstScope(details::attachment_key_c auto key, const AccessScope & scope) noexcept;
    //- only valid when every prior producer wrote the same framebuffer coordinates
    Self & enableByRegionDependency() noexcept;
```

`setInPassUsage` 覆盖 in-pass layout 的默认值（color 段 `eColorAttachment`、ds slot
`eDepthStencilAttachment`），read-only depth 之类要用。现有的 `setLayout` 保留为 escape
hatch。

新增私有成员：

```cpp
    std::vector<std::optional<AccessScope>> m_entry_src_scope_opts;  //- slot indexed
    std::vector<std::optional<AccessScope>> m_exit_dst_scope_opts;   //- slot indexed
    std::vector<AttachmentUsage> m_in_pass_usages;                   //- slot indexed
    bool m_by_region_enabled = false;
```

`m_layouts`（现有）改为从 `m_in_pass_usages` 过 policy 得到，`setLayout` 直接写 `m_layouts`
并把对应的 `m_in_pass_usages` 置 `eNone` 表示「已被显式接管」。

### 4.4 `StaticRenderInfo`

同样加 `setEntryUsage` / `setExitUsage`，转发到
`AttachmentDescriptionInfo::setInitialFinalUsage`，落到 `initialLayout` / `finalLayout`。
mask 保持隐式（驱动补 external dependency）。现有的 `setInitialFinalLayout` 不动。

这样 005 里两条路径的声明段完全一致，见 §8。后续如果要给静态路径生成显式 external
dependency，`m_entry_src_scope_opts` / `m_exit_dst_scope_opts` 里的信息可以直接喂
`SubpassDependencyInfo`——两条路径届时收敛到同一份声明，不需要再改 API 形状。

### 4.5 `RenderTarget` 新增 slot 访问

barrier 需要 `vk::Image` 和 `vk::ImageSubresourceRange`，现有接口只能按 key 取。
`begin()` 按 slot 遍历，需要：

```cpp
    std::span<const Attachment> viewAttachments() const noexcept { return m_attachments; }
```

`Attachment` 是 move-only，但 `span<const Attachment>` 只读不拷，可用。
`getImage()` 返回 `const Image &`，`Image` 有 `operator vk::Image()`；
`getDescription().getSubresourceRange()` 给 aspect / mip / layer，
与 005 手写的 `color_range` 同一来源。

## 5. `DynamicRender` 内部实现

### 5.1 新增成员

```cpp
    using BarrierList = std::vector<vk::ImageMemoryBarrier2>;
    using SlotList = std::vector<uint32_t>;
private:
    //- baked in create(); begin() only backfills image + subresourceRange per frame
    BarrierList m_entry_barriers;
    BarrierList m_exit_barriers;
    SlotList m_entry_barrier_slots;   //- parallel to m_entry_barriers
    SlotList m_exit_barrier_slots;    //- parallel to m_exit_barriers
    vk::DependencyFlags m_dependency_flags;
```

`m_entry_barrier_slots` 存的是「这条 barrier 对应哪个 attachment slot」。因为被跳过的 slot
不入队（§5.3），barrier 数组的下标不等于 slot 下标，需要这张映射表在 `begin()` 里回填
image。

### 5.2 `create()` 烘 barrier

对每个 slot 算四个 mask 和两个 layout，`image` / `subresourceRange` 留空：

```cpp
vk::ImageLayout in_pass_layout = layouts[slot];
AccessScope in_pass_scope = utils::access_scope_of(in_pass_usages[slot]);

//- entry
vk::ImageLayout entry_layout = policy.resolve(entry_usages[slot]);
AccessScope entry_src = entry_src_scope_opts[slot].value_or(utils::access_scope_of(entry_usages[slot]));
if (entry_layout != in_pass_layout or entry_src.stage_mask) {
    m_entry_barriers.emplace_back()
        .setOldLayout(entry_layout)
        .setNewLayout(in_pass_layout)
        .setSrcStageMask(entry_src.stage_mask)
        .setSrcAccessMask(entry_src.access_mask)
        .setDstStageMask(in_pass_scope.stage_mask)
        .setDstAccessMask(in_pass_scope.access_mask);
    m_entry_barrier_slots.emplace_back(slot);
}

//- exit
if (exit_usages[slot] != AttachmentUsage::eNone) {
    AccessScope exit_dst = exit_dst_scope_opts[slot].value_or(utils::access_scope_of(exit_usages[slot]));
    //- storeOp eDontCare discards the contents, so the old layout can be eUndefined:
    //- mirrors the loadOp-driven entry downgrade and lets the driver skip the copy
    bool discards = descriptions[slot].getStoreOp() == vk::AttachmentStoreOp::eDontCare;
    m_exit_barriers.emplace_back()
        .setOldLayout(discards ? vk::ImageLayout::eUndefined : in_pass_layout)
        .setNewLayout(policy.resolve(exit_usages[slot]))
        .setSrcStageMask(in_pass_scope.stage_mask)
        .setSrcAccessMask(in_pass_scope.access_mask)
        .setDstStageMask(exit_dst.stage_mask)
        .setDstAccessMask(exit_dst.access_mask);
    m_exit_barrier_slots.emplace_back(slot);
}
```

ds slot 的 `discards` 判定要 `storeOp` 与 `stencilStoreOp` 都是 `eDontCare` 才成立——
只丢一个 aspect 不能把整张图的 `oldLayout` 降级。同理入口侧 `loadOp` 推 `eDiscard` 时，
ds slot 需要 `loadOp` 与 `stencilLoadOp` 都是 discard 类。

队列族索引不设，默认 `vk::QueueFamilyIgnored`，与 `Swapchain.cpp` 一致——同队列无所有权转移。

### 5.3 跳过规则

**入口跳过**：`entry_layout == in_pass_layout` 且 `entry_src.stage_mask` 为空。既没有 layout
转换也没有 memory dependency，barrier 是纯开销。

**出口跳过**：`exit_usage == eNone`。

不能跳过的一个反直觉情况：`entry_layout == in_pass_layout` 但 `entry_src` 非空。比如上一帧
在同一 layout 下写过这张图，本帧需要 WAW 依赖——layout 相等但 barrier 必须发。unified
layouts 开启后这种情况会变成常态，所以判定条件里的 `or entry_src.stage_mask` 不是冗余。

全部 slot 都被跳过时整个 `pipelineBarrier2` 不调用。

### 5.4 `begin()` / `end()`

```cpp
void DynamicRender::begin(CommandBufferProxy & cmd, const RenderTarget & render_target) noexcept
{
    auto attachments = render_target.viewAttachments();
    for (auto && [barrier, slot] : stdv::zip(m_entry_barriers, m_entry_barrier_slots)) {
        barrier.setImage(attachments[slot].getImage())
            .setSubresourceRange(attachments[slot].getDescription().getSubresourceRange());
    }
    //- the exit barriers reference the same attachments; fill them here so end() needs no target
    for (auto && [barrier, slot] : stdv::zip(m_exit_barriers, m_exit_barrier_slots)) {
        barrier.setImage(attachments[slot].getImage())
            .setSubresourceRange(attachments[slot].getDescription().getSubresourceRange());
    }
    if (not m_entry_barriers.empty()) {
        vk::DependencyInfo dependency_info;
        dependency_info.setDependencyFlags(m_dependency_flags)
            .setImageMemoryBarriers(m_entry_barriers);
        cmd.pipelineBarrier2(dependency_info);
    }
    //- ... existing per-frame attachment info fill + beginRendering
}

void DynamicRender::end(CommandBufferProxy & cmd) noexcept
{
    cmd.endRendering();
    if (m_exit_barriers.empty()) { return; }
    vk::DependencyInfo dependency_info;
    dependency_info.setImageMemoryBarriers(m_exit_barriers);
    cmd.pipelineBarrier2(dependency_info);
}
```

两处细节：

- 出口 barrier 的 image 在 `begin()` 里就填好，`end()` 不需要再收 `render_target` 参数——
  签名与 `StaticRender::end` 保持一致。
- `vk::DependencyInfo` 在栈上构造，不做成员。做成员的话 `pImageMemoryBarriers` 会指向
  `m_exit_barriers` 的缓冲区，`DynamicRender` 一旦被 move 就悬空。栈上构造每帧一次
  `setImageMemoryBarriers`，成本可忽略，与 `m_rendering` 现有的 `setColorAttachments`
  per-frame 调用同一处理方式。
- `m_dependency_flags` 只加在入口 barrier 上。出口 barrier 的消费者（blit / transfer）通常在
  framebuffer 空间之外，`eByRegion` 不成立。

### 5.5 达到的最优程度

局部最优，四项：

| 项 | 效果 |
|---|---|
| barrier 数组 create 时烘好 | 每帧零分配，只回填 2 个字段 |
| 一次 `pipelineBarrier2` 覆盖全部 attachment | 驱动可合并 cache 操作；手写时每加一个 attachment 多一次 flush |
| `loadOp`/`storeOp` 驱动的 layout 降级 | 驱动跳过解压与内容搬移；同时消掉一类静默数据丢失 bug |
| 空 barrier 跳过 | unified layouts 开启后大量命中 |

批量提交是把转换收进 `DynamicRender` 的主要性能理由：它是唯一同时看到 color、resolve、
depth/stencil 全部 slot 的地方。

做不到的（需要跨 pass 视野，见 §10）：相邻 pass 的出口/入口 barrier 合并成一条、按实际
读写范围收窄 subresource、`eByRegion` 的自动判定。

## 6. 为什么不做自动 layout 追踪

在 `Image` 或 `Attachment` 上挂 `m_current_layout`，`begin()` 自己读上一次状态——看起来最省事，
不做。三个理由：

- `RenderTarget` 按 `const &` 传，追踪需要 mutable 状态，破坏 const 语义。
- 多帧 in-flight 时同一张 image 会被不同线程的 record 看到不一致的状态。layout 的正确顺序是
  **command buffer 内的记录顺序**，不是墙上时间，per-resource 的单个变量表达不了。
- 这套东西最后会被 RenderGraph 的 pass 级追踪替掉。现在铺开是白做，而且会留下一个看起来
  能用、实际在多线程下错的接口。

意图声明是显式的，多帧之间不携带状态，天然没有这些问题。

## 7. unified image layouts

`VK_KHR_unified_image_layouts`（SDK 1.4.341 起可用，
`vk::PhysicalDeviceUnifiedImageLayoutsFeaturesKHR::unifiedImageLayouts`）。开启后 `eGeneral`
可以用在原本需要特定 layout 的绝大多数位置，且规范承诺无效率损失。

### 7.1 它不能省掉什么

这几条要先说清，否则架构会做歪：

- **memory dependency 照旧。** 扩展干掉的是 layout 要求，不是同步要求。执行依赖、内存可见性、
  WAR / WAW 全都还在，`pipelineBarrier2` 该发还得发。
- **image barrier 仍优于 global barrier。** 提案明确说明：即使 src 与 dst layout 都是
  `eGeneral`，部分硬件上 image barrier 仍是获得最佳性能所必需的。所以 §5 的实现不改用
  `vk::MemoryBarrier2`。
- **`eUndefined → eGeneral` 的首次转换还得有。** `eUndefined` 承载 metadata 初始化与「内容
  可丢弃」语义，规范鼓励继续使用；替代它的 metadata-init API 被推到了未来的扩展。
- **present 仍需 `ePresentSrcKHR`。** compositor 在 Vulkan 之外，扩展未覆盖；
  `eSharedPresentKHR` 同理。
- **video layout 由独立的 `unifiedImageLayoutsVideo` 控制。** 视频单元常与 graphics/compute
  不统一，是单独的 feature 位。
- **`ePreinitialized` 不接受 `eGeneral`。** 与本设计无关（attachment 不走 host 初始化），
  但 `ImageLayoutPolicy` 如果以后扩到普通 image 要注意。
- **驱动支持面窄。** 2025 年的扩展。桌面 AMD / NV 有，移动端 tiler 因 framebuffer 压缩不一定
  给。必须运行期判定，且**特定 layout 那条路径不能退化成没人测的死代码**——它才是移动端会
  走的路径。

### 7.2 开启后的收益

`ImageLayoutPolicy::resolve()` 让 in-pass 与出口 layout 都变成 `eGeneral`，于是：

- 跨 pass 的入口 barrier：`oldLayout == newLayout == eGeneral`，退化成纯 memory dependency。
  驱动不做解压、不刷压缩元数据，比带转换的便宜。
- 首帧的 `eUndefined → eGeneral` 保留（丢弃语义），这一条不变。
- 出口 barrier：layout 相等，只剩 `colorAttachmentWrite → transferRead` 的可见性。

mask 完全不受影响，因为它们从 `AttachmentUsage` 推、不经过 layout（§3）。这是意图声明设计
在这里换来的直接好处：开关 unified layouts 是一行 policy 改动，不触碰同步逻辑。

静态路径同样受益——`AttachmentDescriptionInfo::setInitialFinalUsage` 共用同一个 policy，
RenderPass 的 `initialLayout` / `finalLayout` 一起变 `eGeneral`，减少驱动侧的隐式转换。

### 7.3 阻塞项：Swapchain 硬编码 layout

`Swapchain.cpp:118` 的 blit 把源 layout 写死成 `eTransferSrcOptimal`。如果 render target 的
出口意图经 policy 解析成 `eGeneral`，实际 layout 是 GENERAL 而 blit 声明的是
TRANSFER_SRC_OPTIMAL——不匹配。

`Swapchain` 内部的三处 layout（`:92`、`:100`、`:118`、`:120`）都要改走同一个 policy，
才能开 unified。**这是开启 unified layouts 的硬前置条件**，独立于本文档的主体改动。
在它完成之前，`ImageLayoutPolicy` 默认关闭、`register_unified_image_layouts` 不实现。

## 8. 005 示例改动

`attachment_set` / `render_target` / `image` 那几段（`:167-207`）不动——路径无关。

```diff
     vkc::AttachmentSetInfoBuilder attachment_set_builder;
     vkc::ColorAttachmentKey color_key = attachment_set_builder.addColorAttachment();
-    vkc::AttachmentSetInfo attachment_set = attachment_set_builder.build();
+    vkc::AttachmentSetInfo attachment_set = attachment_set_builder.build(device_context.getImageLayoutPolicy());
```

`build()` 现在要 policy，所以这一段必须移到 `device_context.create()` 之后。005 里
attachment set 的声明在 `:167`、device context 在 `:131`，顺序已经满足。

声明段两条路径逐字相同：

```cpp
    static_render_info.setLoadStoreOp(color_key, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore)
        .setExitUsage(color_key, vkc::AttachmentUsage::eTransferSource)
        .addSubpass(std::move(subpass_info));
```

```cpp
    dynamic_render_info.setLoadStoreOp(color_key, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore)
        .setExitUsage(color_key, vkc::AttachmentUsage::eTransferSource);
```

入口不写——`loadOp = eClear` 自动推出 `eDiscard`。出口 blit 想要精确的 `eBlit` stage 而不是
`eAllTransfer`，加一行：

```cpp
        .setExitDstScope(color_key, {vk::PipelineStageFlagBits2::eBlit, vk::AccessFlagBits2::eTransferRead})
```

渲染循环里 `:289-318` 那 28 行全删，剩下：

```cpp
    dynamic_render.begin(cmd, render_target);
    dynamic_graphics_pipeline.bind(cmd);
    cmd.draw(3, 1, 0, 0);
    dynamic_render.end(cmd);
```

`present_image` 那行（`:328`）恢复成从 render_target 直接取（`color_attachment` 局部变量
不再存在）。

### 8.1 跨帧行为

值得单独说明，这是原来手写 barrier 时靠注释维持的隐含约定：

- 帧 0：image 从 `eUndefined` 进来（`setInitialLayout(eUndefined)` 创建），入口 barrier
  `eUndefined → eColorAttachmentOptimal`，合法。
- 帧 N>0：上一帧的出口 barrier 把它留在 `eTransferSrcOptimal`，present 的 blit 只读不改
  layout。入口意图仍是 `eDiscard`（`eUndefined`），仍然合法——`loadOp = eClear` 不需要旧内容。

所以整个循环不需要追踪实际 layout，`eDiscard` 每帧都成立。一旦某帧改成 `loadOp = eLoad`，
入口意图自动变成 in-pass 意图（§4.2），此时 `oldLayout` 就是 `eColorAttachmentOptimal`，
与实际的 `eTransferSrcOptimal` 不符——**这种情况必须显式 `setEntryUsage(key, eTransferSource)`**。
文档记录在此，代码里由 validation layer 兜（layout 不匹配会报 VUID）。

## 9. 落地顺序

1. `pipeline/graphics/enums.h`（`AttachmentUsage`）+ `utils/access_scope_utils.h`
   （`AccessScope`、`access_scope_of`）—— 独立，无依赖。
2. `utils/ImageLayoutPolicy.h/.cpp` —— 只依赖 1。默认关闭 unified。
3. `DeviceContext` 加 policy 成员与 getter —— 此时 policy 恒为关闭态，行为与现在完全一致。
4. `AttachmentSetInfo` / builder 加 policy 与两个意图数组，`AttachmentDescriptionInfo` 加
   `setInitialFinalUsage` —— `build()` 签名变了，005 要跟着改一行。
5. `StaticRenderInfo::setEntryUsage/setExitUsage` —— 静态路径先接，005 静态路径跑通、
   画面不变，验证 policy 与意图映射正确。**这一步是动态路径的安全网**：静态路径有驱动的
   隐式 barrier 兜底，意图映射错了会体现在 layout 上而不是同步上，更容易定位。
6. `RenderTarget::viewAttachments()`。
7. `DynamicRenderInfo` 的六个新接口 + `DynamicRender` 烘 barrier / `begin` / `end`。
8. 005 删 28 行 barrier，动态路径跑通。开 validation layer，重点看
   `VUID-vkCmdBeginRendering-*`、`VUID-VkImageMemoryBarrier2-oldLayout-*`
   与 sync validation 的 hazard 报告。
9. 单独一步（可延后）：`Swapchain` 三处 layout 改走 policy（§7.3），然后
   `register_unified_image_layouts` + 开 feature。需要支持的驱动才好验证。

`libs/vk_core/CMakeLists.txt` 用 `GLOB_RECURSE`，新增 .cpp 不改 CMake，但要重跑 configure。

## 10. 范围外

- **跨 pass barrier 合并。** pass A 的出口与 pass B 的入口是同一个转换时应合成一条。需要
  RenderGraph 的 pass 依赖图。
- **subresource 收窄。** 现在整个 attachment 的 range 一起转换；只写了部分 mip / layer 时
  可以收窄。需要知道实际写入范围。
- **`eByRegion` 自动判定。** 需要知道前序 producer 是否 framebuffer-local，现在只能 opt-in。
- **静态路径的显式 external dependency。** 意图信息已经具备（§4.4），但生成
  `SubpassDependencyInfo` 需要决定与多 subpass 的交互，另开设计。
- **queue family ownership transfer。** 现在全部 `eQueueFamilyIgnored`。跨队列（async
  compute / transfer queue）要引入 acquire/release barrier 对，与本设计正交。
- **`ImageLayoutPolicy` 扩到非 attachment image。** 采样纹理、storage image 的 layout 也该
  过同一个 policy，否则 unified 只覆盖一半。等纹理路径落地后再做。

## 11. 已知遗留问题

沿用 `vkc-dynamic-render-impl.md` §9 的清单，与本设计相关的两条：

- `RenderTarget` 缺 `setResolveAttachment` / `setDepthStencilAttachment`，所以 resolve 段与
  ds slot 的 barrier 路径在 005 里跑不到，只有单 color 被覆盖。多 attachment 的批量提交
  （§5.5 的主要收益）需要更复杂的示例才能验证。
- resolve 段的槽位顺序是 `unordered_map` 的哈希序，结果正确但跨运行不确定。barrier 数组的
  顺序会跟着变——不影响正确性（barrier 之间无序），但调试时看到的顺序不稳定。

