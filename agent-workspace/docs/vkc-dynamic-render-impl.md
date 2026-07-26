# vkc DynamicRender 实现文档

> 目标：在 `AttachmentSetInfo` 权威模型之上落地 dynamic rendering 路径
> (`DynamicRenderInfo` / `DynamicRender` / `DynamicGraphicPipeline`)，先在 005 跑通。
> 参照 `vkc-attachment-set-authority-design.md` 与已落地的静态路径
> (`StaticRender.h/.cpp`、`info_structs.h:949` 起的 `StaticRenderInfo`)。
> 本文给实现方案与取舍理由，不复制完整代码。

---

## 1. 结论先行：复用 / 新增 / 消失

| 组件 | 动态路径 | 说明 |
|---|---|---|
| `AttachmentSetInfo` + 三种 key | **原样复用** | 路径无关；`DynamicRenderInfo` 已在 `info_structs.h:810` 前置声明、`:826` friend |
| `RenderTargetInfo` | **原样复用** | 只管 format/samples/extent，不知道 render pass 存在 |
| `RenderTarget` | **原样复用** | slot 序 `[colors][resolves][ds?]` 两条路径共用 |
| `GraphicsPipelineInfo` | **原样复用** | 静态版 `create` 里对它全是状态 getter，零 render pass 耦合 |
| `AttachmentDescriptionInfo` | 复用，**缺 2 个 getter** | 见 §5.1 |
| `SubpassDescriptionInfo` / `SubpassDependencyInfo` | **不参与** | 动态路径无 subpass；连带绕开 `m_flat_depth_stencil_ref` 移动后悬垂那个坑 |
| framebuffer / render pass 对象 | **消失** | `StaticRender::begin` 那个按地址做 key 的 `m_framebuffer_cache` 整块不存在 |
| `initialLayout` / `finalLayout` | **消失** | 动态路径只有一个"渲染期布局"，转换归调用方 |
| `DynamicRenderInfo` | 新增 ~70 行头 | §3 |
| `DynamicRender` | 新增 ~45 行头 + ~90 行 cpp | §4 |
| `DynamicGraphicPipeline::create/bind` | 新增 ~75 行 cpp | 目前**只有声明没有定义**，且 `HEAD` 上就如此，不是这轮删出来的 |

净结论：动态路径比静态路径少写 render pass 创建与 framebuffer 缓存，多写 per-slot 布局
数组与 `vk::PipelineRenderingCreateInfo`。总量更小，但**布局转换的责任从驱动移交给调用方**，
这是唯一会实际咬人的地方。

## 2. `imageLayout` 归属：`DynamicRenderInfo` 自持 per-slot 数组

### 2.1 为什么不能复用描述里的 layout 字段

`AttachmentDescriptionInfo` 包的是 `vk::AttachmentDescription2`，只有
`initialLayout`/`finalLayout` 一对（`info_structs.h:738`）。动态路径要的是
`vk::RenderingAttachmentInfo::imageLayout` —— **一个**布局，语义是"渲染期间这张图处于什么
布局"，既不是入口也不是出口。把它挪用进 `initialLayout` 是语义重载：同一字段在静态路径读作
"pass 开始前的布局，驱动会帮你转"，在动态路径读作"渲染期布局，你自己转好"，两种读法在同一个
`AttachmentSetInfo` 上共存，谁都说不清 `getInitialLayout()` 返回的是哪个意思。

### 2.2 定案

per-slot 布局数组归 `DynamicRenderInfo` 自己所有，flat slot 索引，`size() == 描述数`：

```
m_layouts[slot]   // slot 序与 m_attachments.m_descriptions 一致：[colors][resolves][ds?]
```

理由：**动态路径的布局不是 attachment 的静态属性，而是 render 时的瞬时状态**。attachment
的固有事实（format/samples）归 `RenderTargetInfo`，跨帧不变；布局每帧可以不同，取决于上一
个操作是什么。放在 render info 上更诚实。

代价，明确记下来：`DynamicRenderInfo` 不再像 `StaticRenderInfo` 那样"纯粹只往 set 里写"，
它有了自己的状态。两者的对称性到此为止。作为补偿，load/store op、stencil op、flags 仍然
全部写进 set 的描述里，与 `StaticRenderInfo` 完全一致 —— 只有布局这一项例外。

默认值按角色给，让常见情形零配置：

| slot 角色 | 默认布局 |
|---|---|
| color | `eColorAttachmentOptimal` |
| resolve | `eColorAttachmentOptimal` |
| depth stencil | `eDepthStencilAttachmentOptimal` |

005 因此一行 `setLayout` 都不用写。

### 2.3 权威划分更新

| `vk::AttachmentDescription2` 字段 | 权威 | 动态路径是否用 |
|---|---|---|
| `format` / `samples` | `RenderTargetInfo` | 用（喂 `PipelineRenderingCreateInfo`） |
| `loadOp` / `storeOp` | `StaticRenderInfo` / `DynamicRenderInfo` | 用 |
| `stencilLoadOp` / `stencilStoreOp` | 同上 | 用（拆给 `pStencilAttachment`） |
| `flags` | 同上 | **不用**（无 `MAY_ALIAS` 对应物） |
| `initialLayout` / `finalLayout` | `StaticRenderInfo` 独占 | **不用**，改用 `DynamicRenderInfo::m_layouts` |
| `resolveMode` | `AttachmentSetInfoBuilder` | 用 |

字段集依然两两不交，无先后与同步问题。

## 3. `DynamicRenderInfo`

### 3.1 形状

与 `StaticRenderInfo` 同构：私有 `utils::DynamicStructureChain<Root>` 成员 + `Root` 转换
运算符 + `requestExtension<T>()`，**不继承**（24 个 info 类无一继承；公开继承会把
`root()` 的非 const 重载暴露成绕过权威模型的后门，且无虚析构）。

```
Root = vk::RenderingInfo
friend class DynamicRender;
AttachmentSetInfo & m_attachments;   // 引用，与 StaticRenderInfo 一致
LayoutList m_layouts;                // §2.2，构造时按角色填默认值
拷贝删除 / 移动默认
```

构造函数需要按角色填 `m_layouts`，这要求知道 color 段与 ds 段的边界。`DynamicRenderInfo`
已是 `AttachmentSetInfo` 的 friend，直接读 `m_descriptions.size()`、
`getColorAttachmentCount()`、`m_has_depth_stencil` 即可，不必新加公开面。

### 3.2 公开面

写侧，与 `StaticRenderInfo` 逐条对应：

- `setLoadStoreOp(key, load, store)`、`setStencilLoadStoreOp(ds_key, load, store)`、
  `addAttachmentFlags(key, flags)` —— 全部 `m_attachments.mutableAt(key).setXxx()`，
  照抄静态版。
- `setLayout(key, layout)` —— 动态路径独有，写 `m_layouts[getIndex(key)]`。
  **注意 `getIndex` 在 `AttachmentSetInfo` 里是私有的**，friend 身份可直接调。
- `addFlags(vk::RenderingFlags)` —— suspend/resume、`eContentsInlineKHR` 等，写进 chain root。
- `setViewMask(uint32_t)` —— multiview。与 §5.3 的 pipeline 侧必须一致。
- `requestExtension<T>()`。

**不提供** `setInitialFinalLayout` —— 动态路径没有这个概念，提供了就是骗人。

读侧只加真有消费者的：

- `getColorAttachmentCount()` —— 005 拿它构造 `ColorBlendStateInfo`，和静态版同样的理由。

其余一律不加公开 getter：`DynamicRender` 是 friend，直接读私有成员。这里与
`StaticRenderInfo` 有一处不对称 —— 后者既 friend 了 `StaticRender` 又给了
`getSubpasses()`/`getDependencies()`/`getAttachmentDescriptions()` 一套公开 getter。那套
getter 目前唯一的调用者就是 `StaticRender.cpp`，也就是说 friend 已经够了、getter 是多余的。
新写的 `DynamicRenderInfo` 不重复这个冗余；`StaticRenderInfo` 那边保持现状（能跑，且不是
正确性问题）。

## 4. `DynamicRender`

### 4.1 create：纯 CPU 预烘，无 device 对象

没有 render pass 可建，`create` 全是预计算。为了与 `StaticRender::create` 签名对称仍返回
`std::error_code`，但当前实现里没有失败路径 —— 恒返回 `{}`，这点在注释里写明，别让后来人
以为漏了错误处理。

预烘三样：

**(a) `vk::RenderingAttachmentInfo` 模板数组。** `resolveMode` / `imageLayout` /
`resolveImageLayout` / `loadOp` / `storeOp` 全部 create 时定好，`imageView` 与
`resolveImageView` 留空、`begin` 时填。depth 与 stencil 各一个 `std::optional` 模板。

**(b) slot 索引表。** `begin` 需要知道 color i 的 view 在 target 的第几个 slot、它的 resolve
在第几个 slot。前者就是 i（color slot == color 序号），后者取
`m_color_resolve_list[i].second`，无 resolve 时是 `vk::AttachmentUnused`。**resolve slot 是
存下来的哈希序，不能用 `i + color_count` 推算** —— 这个坑静态路径已经踩过一次
(`RenderTarget::getIndex(ResolveAttachmentKey)` 的修法)。

**(c) format 数组。** `pColorAttachmentFormats` 要的是 color 段的 format（多重采样那份，
resolve 目标 format 必须与之相同），加 depth/stencil format。见 §5.2、§5.3。

**关键：pNext 链必须自己持一份拷贝。** `begin` 每帧都要重建 `vk::RenderingInfo`，其
`pNext` 指向 `DynamicRenderInfo` 的 chain 节点。若 `create` 只存下 root 的一份值拷贝，
`DynamicRenderInfo` 析构后 `pNext` 立刻悬垂 —— 静态路径不会遇到，因为
`createRenderPass2Unique` 当场消费完就不再引用。做法是让 `DynamicRender` 持一个
`utils::DynamicStructureChain<vk::RenderingInfo>` 成员，从 info 的 chain **拷贝构造**：
`DynamicStructureChain` 的拷贝构造会 `relink()` 修正 `pNext`
(`DynamicStructureChain.h:27`)，正是要的语义。这样 `DynamicRender` 与 `StaticRender` 一样，
create 之后不再依赖 info 存活。

> 注意 `relink()` 只修 `pNext`，不修结构体内部的数组指针 —— 这里只有 `pNext` 需要修，
> `pColorAttachments` 等由 `begin` 每帧重填，不受影响。

### 4.2 begin：填 view 与 clear value

模板数组作为 `mutable` 成员就地改写，零分配：

```
for i in [0, color_count):
    m_color_attachments[i].imageView  = views[i]
    m_color_attachments[i].clearValue = clear_values[i]
    if resolve_slot[i] != AttachmentUnused:
        m_color_attachments[i].resolveImageView = views[resolve_slot[i]]
```

`views` 取 `render_target.viewAttachmentImageViews()`（`transform_view` over `vector`，
random access，`operator[]` 可用），`clear_values` 取 `getClearValues()` —— 两者都已按 slot
排好，动态路径无需新增 `RenderTarget` 接口。

depth / stencil 模板填同一个 ds slot 的 view，clear value 也取同一个 slot；两者的 op 来自
同一份描述的不同字段对（见 §5.4）。

然后 `vk::RenderingInfo` 从 §4.1 那份 chain 的 root 拷出，补 `renderArea` /
`layerCount` / 三个 attachment 指针，`cmd.beginRendering(...)`。`end` 就是
`cmd.endRendering()`。

`CommandBufferProxy` 公开继承 `vk::CommandBuffer`
(`CommandBufferProxy.h:22`)，`beginRendering` / `endRendering` / `pipelineBarrier2`
直接可用，不需要包一层。

### 4.3 与静态路径的形状差异

**resolve 从并列变嵌套。** 静态路径 resolve 是 flat 数组里的独立 slot，靠
`pResolveAttachments` 与 `pColorAttachments` 并列；动态路径长在每个 color 的
`RenderingAttachmentInfo` 内部。set 里的 resolve slot 仍然有用 —— 持有 resolve 图像的
format/samples，并给 `RenderTarget` 一个存 `Attachment` 的位置 —— 只是引用方式变了。

顺带：静态路径那条"resolve 数量必须等于 color 数量，否则驱动按共享的
`colorAttachmentCount` 读越界"的约束在动态路径**不存在**，因为每个 color 自带 resolve 槽，
没有共享计数。动态路径可以只给部分 color 配 resolve。

**depth 与 stencil 拆成两个结构。** `pDepthAttachment` 与 `pStencilAttachment` 各有独立的
op / clearValue / imageLayout，而 set 里只有一个 ds slot、一份描述。`begin` 把那一份拆开：
depth 取 `loadOp`/`storeOp`，stencil 取 `stencilLoadOp`/`stencilStoreOp`。现成的
`setStencilLoadStoreOp` 存的正好够用。

若 format 只含 depth 或只含 stencil，对应的那个指针置空 —— 需要 §5.2 的 aspect 判定。

## 5. 配套缺口

### 5.1 `AttachmentDescriptionInfo` 缺 stencil op getter

现有 getter 到 `getFinalLayout()` 为止（`info_structs.h:743-748`），没有
`getStencilLoadOp()` / `getStencilStoreOp()`。静态路径不需要 —— 它整体
`static_cast<vk::AttachmentDescription2>` 就带走了全部字段；动态路径要把一份描述**拆**成
depth 和 stencil 两个结构，必须逐字段读。这两个 getter 有真实消费者，补。

`getFlags()` 不补：动态路径不用 flags，静态路径走整体转换。

### 5.2 format → aspect 判定

两处需要：ds attachment 建 view 时的 `aspectMask`，以及判断 ds format 是否含 stencil 以决定
`pStencilAttachment` 是否置空。

不要手写 format 白名单。`<vulkan/vulkan_format_traits.hpp>` 里的
`vk::componentCount(format)` + `vk::componentName(format, i)` 已经给出答案：depth 分量名是
`"D"`，stencil 是 `"S"`（如 `eD24UnormS8Uint` 返回 `"D"`, `"S"`）。写一个
`vk::ImageAspectFlags aspect_of(vk::Format)` 放在 `vk_core/utils` 下，`constexpr` 可用。
005 不含 ds，此项可后置，但 006 一上 depth 就需要。

### 5.3 `DynamicRenderScopeInfo` 的生命周期

`vk::PipelineRenderingCreateInfo` 需要 color format 数组
（`viewMask` / `colorAttachmentCount` / `pColorAttachmentFormats` /
`depthAttachmentFormat` / `stencilAttachmentFormat`），且必须与 draw 时的
`vk::RenderingInfo` 对得上。

`StaticRenderScopeInfo` 是 handle + int，随便拷贝；`DynamicRenderScopeInfo` 带数组，
**不能天真地按值存一个内部有指针的 `vk::PipelineRenderingCreateInfo`**，否则拷贝即悬垂。
做法：scope 只持 `std::span<const vk::Format>` 指回 `DynamicRender` 拥有的 vector，
外加两个 format 与 viewMask；那个结构体由 `DynamicGraphicPipeline::create` 现场组装挂 pNext。
pipeline 创建是同步完成的，`DynamicRender` 显然还活着，生命周期天然安全。

`vk::StructExtends<PipelineRenderingCreateInfo, GraphicsPipelineCreateInfo>` 存在
（`vulkan.hpp:12068`），但 `GraphicsPipelineInfo` 用的是平铺成员不是 chain，所以直接
`.setPNext(&rendering_create_info)` 挂在局部 `vk::GraphicsPipelineCreateInfo` 上即可。

两个坑：

- **`PipelineRenderingCreateInfo` 没有 samples 字段。** 采样数只来自 `pMultisampleState`。
  静态路径下 render pass 会校验描述的 samples 与 pipeline multisample state 是否一致；动态
  路径下除了 validation layer 没人管。005 现在根本没设 multisample，靠
  `MultisampleStateInfo` 默认的 `e1`（`info_structs.h:220`）蒙对，MSAA 一上来这里就是哑坑。
  建议 006 显式 `setMultisampleStateInfo`，值取 `render_target_info.getSampleCount()`。
- **`viewMask` 两处必须一致**，否则 UB。这是 `DynamicRenderInfo::setViewMask` 与
  `makeScopeInfo` 共用同一份数据的原因。

### 5.4 `DynamicGraphicPipeline::create` 与静态版重复

去掉 `setRenderPass`/`setSubpass`、加个 pNext 之后，其余约 60 行（shader module 创建、
pipeline layout、九个 state 指针）与 `StaticGraphicsPipeline::create` 逐字相同。

先照抄跑通，不要为此提前抽象。等两条路径都稳定了再决定抽哪一层 —— 现在抽会把还没定型的
scope 概念一起焊死。记在这里免得被当成疏漏。

顺带：类名是 `DynamicGraphicPipeline`（`DynamicGraphicsPipeline.h:9`，少个 s，与文件名和
`StaticGraphicsPipeline` 不一致）。要改就趁现在没有调用者。

### 5.5 `RenderTarget` 缺两个 setter（两条路径共有）

现在只有 `setColorAttachment`，缺 `setResolveAttachment(ResolveAttachmentKey, ...)` 与
`setDepthStencilAttachment(DepthStencilAttachmentKey, ...)`（后者要按 format 推 aspect，
见 §5.2）。私有的 `getIndex` 三个重载都已就位，setter 本身是照抄
`setColorAttachment` 换 aspect。

**005 不需要**（单 color、无 resolve、无 ds）。006 只要不是同样的最简形状就绕不过去。

## 6. 在 005 跑通

### 6.1 路径无关的部分：一行不改

L163-204 那段（`AttachmentSetInfoBuilder` → `color_key` → `attachment_set` →
`RenderTargetInfo` → image 创建 → `render_target.build` → `setColorAttachment`）动态路径
完全照用。这是权威模型的直接收益。

### 6.2 要改的四处

**(a) 启用 feature。** 与现有 `synchronization2` 那行并列加一条：

```cpp
device_ext_manifest.addRequiredFeature(vkc::utils::t_feature_bit<&vk::PhysicalDeviceVulkan13Features::dynamicRendering>)
```

库里不用改。API version 已是 `vk::HeaderVersionComplete`，`vulkanApiVersion` 是 1.4
（`Allocator.cpp:53`），核心特性可用，不需要 `VK_KHR_dynamic_rendering` 扩展。

**(b) render 声明。** 替掉 `SubpassDescriptionInfo` + `StaticRenderInfo` +
`StaticRender`（当前 L208-220）：

```cpp
vkc::DynamicRenderInfo dynamic_render_info {attachment_set};
dynamic_render_info.setLoadStoreOp(color_key, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore);
vkc::DynamicRender dynamic_render;
if (auto ec = dynamic_render.create(dynamic_render_info)) { ... }
```

没有 subpass，没有 `setInitialFinalLayout` —— 布局默认就是
`eColorAttachmentOptimal`，正是要的值，一行都不用写。`create` 也不再需要 `device`。

**(c) pipeline。** `static_render.makeScopeInfo(0)` 换
`dynamic_render.makeScopeInfo()`（无 subpass 索引），`StaticGraphicsPipeline` 换
`DynamicGraphicPipeline`。`ColorBlendStateInfo{dynamic_render_info.getColorAttachmentCount()}`
不变。

**(d) 渲染循环里补两道 barrier。** 这是唯一有实质工作量的改动。

### 6.3 barrier：动态路径必须自己转布局

静态路径靠 render pass 的 `eUndefined → eTransferSrcOptimal` 白拿了两次转换。动态路径没有
这个机制。而 `Swapchain::present` 只 barrier swapchain 自己那张图，源图要求**调用方已经放在
`eTransferSrcOptimal`**（`Swapchain.cpp:118` 的 `setSrcImageLayout`）。所以必须补：

```
cmd.begin
  barrier: eUndefined -> eColorAttachmentOptimal
           srcStage eTopOfPipe, srcAccess {} / dstStage eColorAttachmentOutput, dstAccess eColorAttachmentWrite
  dynamic_render.begin(cmd, render_target)
  pipeline.bind / draw
  dynamic_render.end(cmd)
  barrier: eColorAttachmentOptimal -> eTransferSrcOptimal
           srcStage eColorAttachmentOutput, srcAccess eColorAttachmentWrite / dstStage eTransfer, dstAccess eTransferRead
cmd.end
```

`oldLayout` 用 `eUndefined` 而不是上一帧的 `eTransferSrcOptimal`：loadOp 是 `eClear`，内容
不需要保留，丢弃旧内容是正确且更快的。两张 target 交替使用也因此不必区分首帧。

`Image` 不跟踪 layout（有意的设计：`vk::Image` 可共享，layout 是 per-subresource 的共享
事实，放进可拷贝的 `Image` 会分裂成两份状态），所以这两道 barrier 只能手写。
subresource range 取 `render_target.getAttachment(color_key).getDescription().getSubresourceRange()`，
image 取同一个 attachment 的 `getImage()`。`synchronization2` 已启用，用
`pipelineBarrier2` + `vk::ImageMemoryBarrier2`。

库里**不加** barrier 辅助（`vk_core/sync` 下目前只有 timeline semaphore）。什么时候需要
barrier、用什么 stage/access，是 RenderGraph 层的判断，不是 `DynamicRender` 能替调用方决定
的 —— 它连"这张图上一步被谁用过"都不知道。先在例子里手写，等 006/007 出现重复形状再看要不
要抽。

### 6.4 005 改造 vs 新建 006

建议**直接把 005 改成动态路径跑通验证**，绿了之后再决定是拆成 006 还是保留双路径。理由：
静态版已经 commit（`5b8770d`），git 里跑不掉；而在同一个文件里用 `if constexpr` 挂两条路径
会让 barrier 那段变得难读 —— 静态路径不需要 barrier，动态路径需要，这个差异没法优雅地
条件化。

验证点，按顺序：

1. validation layer 无报错（尤其 layout 转换与 `PipelineRenderingCreateInfo` 的 format 匹配）
2. 画面与静态版一致（白底三角）
3. 两张 target 交替、`present_blit_finish_tokens` 逻辑不变

## 7. 落地顺序

1. `AttachmentDescriptionInfo` 补两个 stencil op getter（§5.1）
2. `DynamicRenderInfo`（§3）—— 纯头文件，`m_layouts` 构造时填默认值
3. `DynamicRender.h/.cpp`（§4）—— chain 拷贝那处是唯一需要想清楚的地方
4. `DynamicGraphicPipeline::create/bind`（§5.4）—— 照抄静态版
5. 005 改造（§6）—— feature bit、render 声明、pipeline、两道 barrier
6. 跑通后再补 `RenderTarget` 两个 setter（§5.5）与 aspect helper（§5.2），为 006 的
   depth/MSAA 铺路

1-5 是跑通 005 的最小集。`libs/vk_core/CMakeLists.txt` 用 `GLOB_RECURSE`，新增 .cpp 不用改
CMake。

## 8. 范围外的已知问题

不在本次改动内，记录以免重复发现：

- `SubpassDescriptionInfo::m_flat_depth_stencil_ref` 是按值成员（`info_structs.h:707`）而地址
  存进了 root，移动/拷贝后悬垂。**动态路径碰不到**，静态路径仍在。修法：改成 0/1 元素 vector。
- `AttachmentSetInfoBuilder::build()` 里 resolve 段的顺序来自 `unordered_map` 遍历
  （`info_structs.cpp:60`），正确但不确定。
- chain 包装类的拷贝构造只 relink `pNext`，不修内部数组指针（`DynamicRender` 用得到的那部分
  恰好只需要 `pNext`，见 §4.1 注）。
- `StaticRender` 的 framebuffer 缓存按 target 地址做 key，target 析构后地址复用会取到错误
  的 framebuffer。动态路径无此问题。
- key 的构造函数是公开的（`info_structs.h:758` 等），可以伪造一个 set 内但越界的 index。
- `DynamicGraphicPipeline` 类名少个 s（§5.4）。
