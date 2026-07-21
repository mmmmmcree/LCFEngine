# vkc AttachmentSet + RenderScope 双层设计 (v3)

> 目标：用**同一份描述信息**同时驱动 render pass 与 dynamic rendering 两条路，不隐藏 Vulkan 能力。
> v2 确立了「`AttachmentSetInfo` 是 attachment 池的单一权威」；v3 在其上补一层 **`RenderScopeInfo`**
> ——一次绘制阶段的「选择枢纽」，它是 `VkSubpassDescription2` 与 `VkPipelineRenderingCreateInfo`
> 共同描述面的**并集超集**，向两条路各自投影。
> 本文取代 v2，是对 [`vkc-rendertarget-rendering-design.md`](./vkc-rendertarget-rendering-design.md) §3 的最终演进。
> 给设计与类定义（接口 + 成员布局），不复制完整实现。

---

## 1. 全景：三层模型

```
AttachmentSetInfo          ── 池权威：N 槽 + 角色 + format/samples/load-store/layout
   │                          canonical 布局 [colors][ds?][resolve]；两条路共享同一批 image
   │
   ├─ RenderTargetInfo(set&)   ── image-intrinsic 半 + 绑定：setFormat(index,fmt) / setSampleCount(range)
   │                             / extent / imageView；1 pass : N target
   │
   └─ RenderScopeInfo(set&)    ── 选择枢纽（= 去掉 RenderPass 包袱的 subpass）：
        │                         有序 color 引用 + depth 引用 + input 引用 + resolve 链接 + viewMask
        │                         独立于 VkRenderPass 存在；pipeline 与两条 begin 都消费它
        │            ├─ 投影 → vk::SubpassDescription2            (RP 路径)
        │            └─ 投影 → vk::PipelineRenderingCreateInfo    (dynamic 路径，pipeline 侧)
        │                       + 每 attachment 的 layout/resolve   (dynamic 路径，begin 侧)
        │
        └─ RenderPassInfo(set&)  ── RP 图层超集：有序 [RenderScopeInfo] + dependency + 派生 preserve → VkRenderPass
                                    单 scope 场景默认由一个全覆盖 scope 带出（取代当前 RenderingInfo）

Pipeline::create(dev, pipelineInfo, scopeSource)  ── 两路统一入口：scope 给 formats/viewMask/blend-count
    RP:      scopeSource = renderPass + subpassIndex（内部取该 subpass 的 scope）
    dynamic: scopeSource = RenderScopeInfo（内部合成 PipelineRenderingCreateInfo，renderPass=nullptr）
```

**为什么 scope 不能当 attachment 数据的权威**：attachment 的身份与 format/samples（即那批共享 image）活在
scope **之上**——多个 scope 引用同一批 image。若 scope 当权威，同一 attachment 的 format 会在每个引用它的
scope 里重复，drift 又回来。所以是**两层**：set 定义身份/格式，scope 选择并排布。

## 2. 两条路描述的是同一件事，只是绑定时机不同

pipeline 在两条路里都只需绑定「一个绘制阶段用到哪些 attachment、什么格式、什么顺序」：

- **render pass 路径**：`(VkRenderPass, subpassIndex)` 定位到一个 `VkSubpassDescription2`；format 实际存在
  `VkAttachmentDescription2`，subpass 只存 index+layout。
- **dynamic 路径**：`VkPipelineRenderingCreateInfo{ pColorAttachmentFormats[], depthAttachmentFormat }`
  直接列 format——dynamic 没有 attachment description 这个中间层。

两者都是「按某个顺序引用一批 attachment」，差别只在 format 从哪读。这正是 `RenderScopeInfo` 要抽出的东西。

### 2.1 「一个 subpass = 一次 begin/end 块」——仅在不含 input attachment 时成立

dynamic rendering **没有 subpass**。精确等价：

| render pass | dynamic (base) | dynamic + `dynamic_rendering_local_read` (VK 1.4 core) |
|---|---|---|
| N 个**独立** subpass | N 个 `begin/end` 块 + 手插 barrier | 同左 |
| N 个 subpass 带 **input attachment**（同像素读上一阶段输出） | ❌ 只能 store→load 绕，付带宽 | 1 个块 + block 内 by-region barrier |

**立场**：vkc 只负责让「同一份 scope 描述在两条路产生等价效果」，barrier 实现还是驱动实现是后端细节。
唯一硬裂缝是 input attachment：base dynamic 做不到，需 `local_read`。故 scope 把 input 建成**并集成员**——
后端不支持时拒绝，而非从 scope 抹掉。

## 3. canonical 布局 `[colors][ds?][resolve]` 与其理由

```
索引:  0 .. C-1        C (若有 ds)      C+has_ds .. end
      [ color 0..C-1 | depth_stencil? | resolve 0..M-1 ]
       └──── 多重采样前缀 [0, C+has_ds) ────┘ └── 恒 e1 ──┘
```

- **depth 紧跟 color**：color 与 depth 都是多重采样，resolve target 恒 `e1`。把两个 MS 槽排成**连续前缀**，
  `setSampleCount` 就是「对前缀 range 统一写」一步搞定；depth 若垫在 resolve 之后，MS 槽会裂成头尾两段。
- **代价**：depth 不再位于 `.back()`。用 `optional<uint32_t> m_depth_stencil_index` 记录「有没有 + 在哪」
  （其值恒 `= m_color_attachment_count`），`at(ds)` 读它取位置，不假设尾元素。
- resolve 段起点 `resolve_base = m_color_attachment_count + (m_depth_stencil_index ? 1 : 0)`。

### 3.1 存在性不变量（决定 get 接口形态）

三个 index 都是 **mint-only**（私有构造 + `friend AttachmentSetInfoBuilder`）+ 带 provenance
（`m_set_id`）。于是构造期就绑死两条：

1. **持有 `DepthStencilAttachmentIndex` ⟹ set 一定有 ds 槽**——该 index 只可能来自
   `enableDepthStencilAttachment()`，它必然置上 `m_depth_stencil_index`，`build()` 必然 append ds 槽。
2. **持有 `ResolveAttachmentIndex` ⟹ set 一定有对应 resolve 槽**——只可能来自 `addResolveAttachment()`，
   必然 push 一条 resolve spec，`build()` 必然 append resolve 槽。

结论：**`at(Index)` 返回非 optional `const &`**——「有 index」即「存在性证明」，让已证明存在的调用方再解包
optional 是背叛不变量。optional 只属于「不知道有没有」的 discovery 层（§4.2）。

## 4. 类型定义

### 4.0 三个 index（mint-only + provenance，行为对称）

```cpp
enum class AttachmentSetId : uint32_t {};
inline AttachmentSetId next_attachment_set_id() noexcept;   // 静态原子发号，build() 轮换

// color / resolve 是 positional，存下标；ds 单例、位置归 set，只带 provenance
class ColorAttachmentIndex        { friend AttachmentSetInfoBuilder; friend AttachmentSetInfo;
    ColorAttachmentIndex(uint32_t, AttachmentSetId); uint32_t m_index; AttachmentSetId m_set_id; };
class ResolveAttachmentIndex      { /* 同上 */ };
class DepthStencilAttachmentIndex { friend AttachmentSetInfoBuilder; friend AttachmentSetInfo;
    explicit DepthStencilAttachmentIndex(AttachmentSetId); AttachmentSetId m_set_id; };
```

### 4.1 `AttachmentDescriptionInfo`（已存在，chain 封装 `vk::AttachmentDescription2`）

聚合 image-intrinsic（format/samples）+ render-scope（load/store/layout）+ `m_resolve_mode`（非
`AttachmentDescription2` 字段，仅 dynamic 用）。已具备两个投影：`operator const vk::AttachmentDescription2 &`
（RP 路径）与 `operator vk::RenderingAttachmentInfo`（dynamic 路径，投影 loadOp/storeOp/resolveMode；
imageView/imageLayout/clearValue 是 runtime，留给 begin 填）。**保留现状，不改。**

### 4.2 `AttachmentSetInfo`（池权威，range view 暴露分区）

去掉 `AttachmentFormatRef` / `AttachmentStateRef`——写入直接由 `RenderTargetInfo::setFormat(index, fmt)`
和 `RenderScopeInfo` 的 stateRef 承担（见 §4.4）。set 只暴露**只读 range view** + **discovery 吐 key**。

```cpp
class AttachmentSetInfo
{
    friend class AttachmentSetInfoBuilder;
    friend class RenderTargetInfo2;
    friend class RenderScopeInfo;
    using Self = AttachmentSetInfo;
    using DescriptionList = std::vector<AttachmentDescriptionInfo>;
public:
    ~AttachmentSetInfo() noexcept = default;
    AttachmentSetInfo(const Self &) = delete;              // 持 ref 期间禁拷贝
    AttachmentSetInfo(Self &&) noexcept = default;
    Self & operator=(const Self &) = delete;
    Self & operator=(Self &&) noexcept = default;

    // ── keyed 访问：非 optional，index 即存在性证明（§3.1）；release 编译掉断言 ──
    const AttachmentDescriptionInfo & at(ColorAttachmentIndex k) const noexcept;        // m_descriptions[k.m_index]
    const AttachmentDescriptionInfo & at(DepthStencilAttachmentIndex k) const noexcept; // m_descriptions[*m_depth_stencil_index]
    const AttachmentDescriptionInfo & at(ResolveAttachmentIndex k) const noexcept;      // m_descriptions[resolveBase()+k.m_index]

    // ── discovery：optional 包 index（不是 desc）；有 ds 才吐 key，泛型消费方回喂 at() ──
    std::optional<DepthStencilAttachmentIndex> getDepthStencilIndex() const noexcept;   // set 是 index friend，可盖章铸造
    uint32_t getColorAttachmentCount() const noexcept   { return m_color_attachment_count; }
    uint32_t getResolveAttachmentCount() const noexcept { return static_cast<uint32_t>(m_descriptions.size()) - resolveBase(); }
    bool hasDepthStencil() const noexcept               { return m_depth_stencil_index.has_value(); }
    ColorAttachmentIndex   colorIndex(uint32_t i) const noexcept;    // 断言 i<color_count，返回盖章 key
    ResolveAttachmentIndex resolveIndex(uint32_t i) const noexcept;  // 断言 i<resolve_count

    // ── range view：分区切片，供 RenderTargetInfo 批量写 / create 侧遍历读 ──
    //  多重采样前缀 = colors + ds，正是 setSampleCount 的作用域
    std::span<const AttachmentDescriptionInfo> multisampleSlots() const noexcept
    { return { m_descriptions.data(), m_color_attachment_count + (m_depth_stencil_index ? 1u : 0u) }; }
    std::span<const AttachmentDescriptionInfo> colorSlots() const noexcept
    { return { m_descriptions.data(), m_color_attachment_count }; }
    std::span<const AttachmentDescriptionInfo> resolveSlots() const noexcept
    { return { m_descriptions.data() + resolveBase(), getResolveAttachmentCount() }; }
    std::span<const AttachmentDescriptionInfo> allSlots() const noexcept { return m_descriptions; }
private:
    AttachmentSetInfo() noexcept = default;                 // 仅 builder 可造
    uint32_t resolveBase() const noexcept { return m_color_attachment_count + (m_depth_stencil_index ? 1u : 0u); }
    AttachmentDescriptionInfo & mutableAt(uint32_t index) noexcept { return m_descriptions[index]; }  // friend 写入用

    DescriptionList m_descriptions;
    AttachmentSetId m_set_id {};
    uint32_t m_color_attachment_count = 0;
    std::optional<uint32_t> m_depth_stencil_index;          // 有值=存在+位置(=color_count)
};
```

> **写接口去哪了**：`RenderTargetInfo2`/`RenderScopeInfo` 是 friend，经私有 `mutableAt(uint32_t)` 拿可变
> 引用改字段（RenderTarget 改 format/samples，Scope 经 stateRef 改 load/store/layout）。终端用户不直接碰
> `AttachmentDescriptionInfo` 的 setter，权威唯一。

### 4.3 `AttachmentSetInfoBuilder`（只授权形态；build 后重置可复用）

`add*` 只在 builder 上，`build()` 返回的 `AttachmentSetInfo` 没有 `add*`——「build 完还 add」编译期不可能。
布局在 `build()` 里落定为 `[colors][ds?][resolve]`。

```cpp
class AttachmentSetInfoBuilder
{
    using Self = AttachmentSetInfoBuilder;
public:
    ColorAttachmentIndex addColorAttachment() noexcept;                 // push 一个 color 槽
    DepthStencilAttachmentIndex enableDepthStencilAttachment() noexcept; // 置 has_ds（位置留到 build 落定）
    ResolveAttachmentIndex addResolveAttachment(ColorAttachmentIndex resolved,
        vk::ResolveModeFlagBits mode = vk::ResolveModeFlagBits::eAverage) noexcept;  // 记 {源,mode} spec

    AttachmentSetInfo build() noexcept;   // 排布 [colors][ds?][resolve]：
        // 1. m_descriptions 现即 colors；append ds 槽（记 m_depth_stencil_index = color_count）
        // 2. 逐条 resolve spec：把 mode 写到源 color 的 m_resolve_mode；append 一个 e1 的 resolve 槽
        // 3. 盖 m_set_id、move 进 set；builder 自身重置 + 轮换 next_attachment_set_id() 供复用
private:
    std::vector<AttachmentDescriptionInfo> m_descriptions;
    std::vector<std::pair<ColorAttachmentIndex, vk::ResolveModeFlagBits>> m_resolve_specs;
    bool m_has_depth_stencil = false;
    AttachmentSetId m_batch_id = next_attachment_set_id();
};
```

> 注意 build 顺序：ds 必须在 resolve **之前** append，才能让 `m_depth_stencil_index == color_count`
> 且 resolve 段整体在尾部。（当前代码里 ds/resolve 的 append 顺序需与此一致——见 §7 待做项。）

### 4.4 `RenderTargetInfo2`（image-intrinsic 半 + 绑定；直接 setFormat(index,fmt)）

取消 `AttachmentFormatRef`。format 用带 typed index 的 `setFormat` 直接写共享 set；`setSampleCount` 对
`multisampleSlots()` range 整体写——一次覆盖 colors+ds，天然跳过 resolve 段。

```cpp
class RenderTargetInfo2
{
    using Self = RenderTargetInfo2;
public:
    explicit RenderTargetInfo2(AttachmentSetInfo & set) noexcept : m_set(set) {}
    // 拷贝/移动按需（1 pass : N target，通常各自新建）

    // format：typed index 直写共享 set（单一权威落在 RenderTarget 侧，image 与 RP 都从 set 读同一份）
    Self & setFormat(ColorAttachmentIndex k, vk::Format fmt) noexcept;        // m_set.mutableAt(k.m_index).setFormat
    Self & setFormat(DepthStencilAttachmentIndex k, vk::Format fmt) noexcept; // 经 m_set 私有位置写
    // resolve 槽 format 跟随其源 color，由 create 侧合成，无需用户设

    // sample count：对 multisample 前缀 range 统一写（colors + ds），resolve 恒 e1 不动
    Self & setSampleCount(vk::SampleCountFlagBits s) noexcept;                // for (slot : m_set.multisampleSlots()) slot.setSamples(s)

    Self & setExtent(vk::Extent2D e) noexcept { m_extent = e; return *this; }
    const vk::Extent2D & getExtent() const noexcept { return m_extent; }
    const vk::SampleCountFlagBits & getSampleCount() const noexcept { return m_sample_count; }
    // imageView / clearValue 绑定见 RenderTarget 运行期模型
private:
    AttachmentSetInfo & m_set;                          // 持引用，不拥有
    vk::Extent2D m_extent;
    vk::SampleCountFlagBits m_sample_count = vk::SampleCountFlagBits::e1;
};
```

> `multisampleSlots()` 返回的是 `const` span，`setSampleCount` 需要写——实现里经 friend 的 `mutableAt`
> 按 `[0, prefix)` 循环写，或加一个私有 `mutableMultisampleSlots()` 返回可变 span。const 版对外，可变版内部用。

### 4.5 `RenderScopeInfo`（选择枢纽 = 一等化的 subpass，两路共享）

从 `AttachmentSetInfo` 挑一批 attachment 组成一个绘制阶段，独立于 VkRenderPass 存在。它是
`vk::SubpassDescription2` 与 `vk::PipelineRenderingCreateInfo` 共同面的并集超集。

```cpp
class RenderScopeInfo
{
    using Self = RenderScopeInfo;
public:
    explicit RenderScopeInfo(AttachmentSetInfo & set) noexcept : m_set(set) {}

    // ── 选择（拓扑层，两路都要）──
    Self & addColor(ColorAttachmentIndex k,
        vk::ImageLayout layout = vk::ImageLayout::eColorAttachmentOptimal);        // 有序 color 引用
    Self & setDepthStencil(DepthStencilAttachmentIndex k,
        vk::ImageLayout layout = vk::ImageLayout::eDepthStencilAttachmentOptimal);
    Self & addInput(ColorAttachmentIndex k, vk::ImageLayout, vk::ImageAspectFlags);// RP 原生 / dynamic 需 local_read
    Self & setViewMask(uint32_t mask) noexcept;

    // ── render-scope 状态写回 set（取代 AttachmentStateRef）──
    Self & setLoadStoreOp(ColorAttachmentIndex k, vk::AttachmentLoadOp, vk::AttachmentStoreOp) noexcept;
    Self & setLayouts(ColorAttachmentIndex k, vk::ImageLayout initial, vk::ImageLayout final) noexcept;
    //   depth 版重载同理；这些经 m_set.mutableAt 写共享 set

    // ── 投影（后端各取所需）──
    vk::SubpassDescription2 toSubpassDescription() const;          // RP：color/input/depth ref + pResolveAttachments
    // dynamic 侧：pipeline 从 colorFormats()/depthFormat() 合成 PipelineRenderingCreateInfo；
    //             begin 从 color 引用 + set.at().operator vk::RenderingAttachmentInfo 合成
    std::vector<vk::Format> colorFormats() const;                  // 按 addColor 顺序 = m_set.at(k).getFormat()
    vk::Format depthFormat() const;                                // 无 depth 则 eUndefined
    uint32_t colorReferenceCount() const noexcept;                 // pipeline blend-count 自动推导用
private:
    AttachmentSetInfo & m_set;
    std::vector<AttachmentReferenceInfo> m_color_refs;             // 保留 index+layout
    std::vector<AttachmentReferenceInfo> m_input_refs;
    std::optional<AttachmentReferenceInfo> m_depth_ref;
    uint32_t m_view_mask = 0;
    // resolve 链接从 set 的 m_resolve_mode 派生，无需在 scope 重存
};
```

### 4.6 `RenderPassInfo`（RP 图层超集，取代当前 `RenderingInfo`）

```cpp
class RenderPassInfo
{
    using Self = RenderPassInfo;
public:
    explicit RenderPassInfo(AttachmentSetInfo & set) noexcept;      // 默认带一个全覆盖 defaultScope()
    RenderScopeInfo & defaultScope() noexcept;                      // 单 subpass 场景直接用这个
    Self & addScope(RenderScopeInfo scope);                         // 多 subpass 手动追加
    Self & addDependency(SubpassDependencyInfo dep);
    // create 侧读：全集 AttachmentDescription（从 set 的 allSlots）+ [scope→SubpassDescription2] + deps
    //             preserve attachment 从跨 scope 使用情况派生（推迟，先手写）
private:
    AttachmentSetInfo & m_set;
    std::vector<RenderScopeInfo> m_scopes;
    std::vector<SubpassDependencyInfo> m_dependencies;
    vk::RenderPassCreateFlags m_flags;
};
```

## 5. 005 example：两条路的代码流程

### 5.0 共享前半（两条路完全一致）——建 set + 建 image

```cpp
auto [width, height] = window.getPixelSize();

//- 形态声明：一个 color 槽（005 无 depth、无 resolve）
vkc::AttachmentSetInfoBuilder builder;
vkc::ColorAttachmentIndex color0 = builder.addColorAttachment();
vkc::AttachmentSetInfo attachments = builder.build();       // 布局 = [color0]

//- RenderTarget 侧写 image-intrinsic：format 单一权威落这里
vkc::RenderTargetInfo2 render_target_info(attachments);
render_target_info.setFormat(color0, vk::Format::eR8G8B8A8Unorm);   // typed index 直写，无 FormatRef
render_target_info.setSampleCount(vk::SampleCountFlagBits::e1);     // 对 multisample range 写
render_target_info.setExtent({width, height});

//- 建 image：format 从 set 单一权威取，不再第二次声明
for (auto & image : render_target_images) {
    vk::ImageCreateInfo image_info;
    image_info.setImageType(vk::ImageType::e2D)
        .setFormat(attachments.at(color0).getFormat())             // 单一权威
        .setExtent({width, height, 1u}).setMipLevels(1u).setArrayLayers(1u)
        .setSamples(attachments.at(color0).getSampleCount())
        .setUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc);
    // image.create(...) 不变
}
```

### 5.1 render pass 路径

```cpp
//- RenderPassInfo 持同一个 set，默认已含全覆盖单 scope
vkc::RenderPassInfo render_pass_info(attachments);
render_pass_info.defaultScope()
    .setLoadStoreOp(color0, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore)
    .setLayouts(color0, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferSrcOptimal);

vkc::StaticRendering static_rendering;
static_rendering.create(device, render_pass_info);          // 读 set allSlots + scope→subpass，无 zip

//- pipeline：blend count 由 scope 的 color ref 数自动推导，无需手传 ColorBlendStateInfo{count}
vkc::GraphicsPipelineInfo pipeline_info;
pipeline_info.setShaderProgramInfo(std::move(shader_program_info))
    .setViewportStateInfo(viewport_state_info);
static_graphics_pipeline.create(device, pipeline_info, static_rendering, /*subpass*/0);

//- 循环：begin(cmd, target) 查/建 framebuffer（1 render pass : N target）
static_rendering.begin(cmd, render_target);
// ... draw ...
static_rendering.end(cmd);
```

### 5.2 dynamic rendering 路径

同一个 `attachments` 与 `RenderScopeInfo`；不建 VkRenderPass，scope 直接喂 pipeline，load/store/layout
移到 begin 的 `VkRenderingAttachmentInfo`。

```cpp
//- 直接用 scope，不需要 RenderPassInfo（无 subpass/RP 对象）
vkc::RenderScopeInfo scope(attachments);
scope.addColor(color0, vk::ImageLayout::eColorAttachmentOptimal)
     .setLoadStoreOp(color0, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore);

//- pipeline：dynamic 重载，内部把 scope.colorFormats()/depthFormat() 合成
//  vk::PipelineRenderingCreateInfo 挂 pNext，setRenderPass(nullptr)
vkc::GraphicsPipelineInfo pipeline_info;
pipeline_info.setShaderProgramInfo(std::move(shader_program_info))
    .setViewportStateInfo(viewport_state_info);
static_graphics_pipeline.create(device, pipeline_info, scope);   // dynamic 重载

//- 循环：begin 合成 VkRenderingInfo。每个 color 的 RenderingAttachmentInfo 由
//  set.at(k).operator vk::RenderingAttachmentInfo()（给 loadOp/storeOp/resolveMode）
//  + runtime 填 imageView/imageLayout/clearValue 合成
vkc::DynamicRendering dynamic_rendering(scope);
dynamic_rendering.begin(cmd, render_target);   // → vkCmdBeginRendering(VkRenderingInfo)
// ... draw ...
dynamic_rendering.end(cmd);                    // → vkCmdEndRendering
```

**两路差异总结**：前半（set + image + scope 的 color 选择与 load/store）**完全共享**；分岔只在
①pipeline 传 `(static_rendering, subpassIndex)` vs `scope`；②循环用 `StaticRendering::begin`（framebuffer）
vs `DynamicRendering::begin`（`VkRenderingInfo`）。同一份 `attachments` 与 scope 描述喂两条路。

## 6. 加深度 / 加 resolve = 加数据，不改流程

```cpp
vkc::ColorAttachmentIndex        color0 = builder.addColorAttachment();
vkc::DepthStencilAttachmentIndex depth  = builder.enableDepthStencilAttachment();
vkc::ResolveAttachmentIndex      rslv   = builder.addResolveAttachment(color0);  // MSAA color → resolve
vkc::AttachmentSetInfo attachments = builder.build();   // 布局 = [color0][depth][resolve0]

render_target_info.setFormat(color0, vk::Format::eR8G8B8A8Unorm)
                  .setFormat(depth,  vk::Format::eD32Sfloat);
render_target_info.setSampleCount(vk::SampleCountFlagBits::e4);   // colors+depth 全设 e4；resolve 恒 e1

// RP 路径：defaultScope 自动含 depth reference + resolve 链接（从 set.getResolveMode 派生）
// dynamic 路径：scope.depthFormat() 非空 → PipelineRenderingCreateInfo.depthAttachmentFormat；
//              resolve 走 begin 的 RenderingAttachmentInfo.resolveMode（挂在源 color 上）
```

- **depth**：`setSampleCount` 因 depth 在 multisample 前缀内，自动同步——正是 §3 布局的收益。
- **resolve**：mode 存在源 color 的 `m_resolve_mode`；RP 投影成 `pResolveAttachments`，dynamic 投影成
  `RenderingAttachmentInfo.resolveMode`。blend count 不受 depth/resolve 影响（只数 color ref）。

## 7. 待做项（按依赖顺序）

1. **`AttachmentSetInfo` 布局改 `[colors][ds?][resolve]`**：当前 `build()` 的 append 顺序需确认 ds 在
   resolve 之前（使 `m_depth_stencil_index == color_count`、resolve 段在尾）。成员 `bool m_has_depth_stencil`
   升级为 `std::optional<uint32_t> m_depth_stencil_index`。
2. **`AttachmentSetInfo` 补接口**：`at(3 类 index)` 非 optional、`getDepthStencilIndex()` 吐 optional key、
   `colorIndex/resolveIndex` discovery、四个 range view（`multisampleSlots/colorSlots/resolveSlots/allSlots`）、
   私有 `mutableAt`。删掉空的 public 段。
3. **删 `AttachmentFormatRef` / `AttachmentStateRef`**：写入改由 `RenderTargetInfo2::setFormat(index,fmt)`
   与 `RenderScopeInfo::setLoadStoreOp/setLayouts(index,...)` 承担，经 friend `mutableAt`。
4. **新建 `RenderScopeInfo`**：选择（color/depth/input/viewMask）+ 状态写回 + 投影
   （`toSubpassDescription` / `colorFormats` / `depthFormat` / `colorReferenceCount`）。
5. **`RenderingInfo` → `RenderPassInfo`**：持 set 引用、默认 `defaultScope()`、`addScope`/`addDependency`；
   `create` 读 `allSlots` + scope 投影，去掉当前 `AttachmentStateInfo` 平行数组与 zip。
6. **`RenderTargetInfo2` 落地**：`setFormat` 双重载、`setSampleCount` 写 multisample range、extent。
7. **`StaticRendering::create` 去 `RenderTargetInfo` 形参**：全集 desc 从 set 生成、samples 从 desc 取、
   depth 接入；`begin(cmd, target)` 不变（1 RP : N framebuffer）。
8. **`StaticGraphicsPipeline`**：RP 重载 blend count 从 `scope.colorReferenceCount()` 推导；新增 dynamic
   重载 `create(device, pipelineInfo, const RenderScopeInfo &)`，内部挂 `PipelineRenderingCreateInfo` +
   `renderPass=nullptr`。
9. **`DynamicRendering` begin/end**：合成 `VkRenderingInfo`，每 attachment 由
   `set.at(k).operator vk::RenderingAttachmentInfo()` + runtime imageView/layout/clear 组装。
10. **重写 005 example** 验证两条路（§5）。

**推迟**：depth/stencil resolve（mode 走 pNext）；多 scope 的 dependency 自动派生 + preserve 自动派生
（先手写 `addDependency` / 手工 preserve）；dynamic 多 scope 的 barrier 自动插入（先单 scope）；
`local_read` 下的 input attachment 与 location/input-index 重映射（并集成员已预留，实现推迟）。



