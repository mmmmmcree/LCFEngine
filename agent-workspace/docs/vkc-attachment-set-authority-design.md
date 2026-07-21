# vkc AttachmentSet 单一权威设计 (v2)

> 目标：消除 attachment 描述信息在 `RenderTargetInfo` 与 `RenderingInfo` 间的**平行数组**。
> 单一权威 `AttachmentSetInfo` 只冻结 **attachment 形态（个数 + 角色）**；`RenderingInfo` 与
> `RenderTargetInfo` **各持 set 的引用**，各暴露一个编辑 ref 往共享 set 里填不相交的字段。
> 只有一份数据，结构上杜绝 drift。
> 本文是对 [`vkc-rendertarget-rendering-design.md`](./vkc-rendertarget-rendering-design.md) §3
> 的演进，取代同名 v1 设计（v1 走「投影快照」，v2 改「共享引用」——见 §3 决策记录）。
> 给设计与方案，不复制完整实现。

---

## 1. 动机：平行数组是唯一残余耦合

现状（`StaticRendering::create`）把一个 attachment 的信息拆在两个对象里，用 `zip` 按位置隐式对齐：

```cpp
stdv::zip(rendering_info.getAttachmentStates(), render_target_info.getColorFormats())
```

- format ← `RenderTargetInfo`；load/store/layout ← `AttachmentStateInfo`（在 `RenderingInfo`）。
- 调用方必须保证两列表**同数量、同顺序**。`zip` 取短截断——数量不一致时**静默丢 attachment**，
  连 validation layer 都可能绕过（数量自洽、只是少了一个）。
- 衍生问题：format 双声明（RTInfo 一次 + `ImageCreateInfo` 一次）；samples 在 create 里
  硬编码 `e1`，无视 `RenderTargetInfo::getSampleCount()`；depth 在 static 路径完全没接。

职责分层本身没错（format 是资源层、load/store 是 pass 语义层）。错的是**没有任何一方是 attachment
集合的权威**，count 与顺序靠人工同步。

## 2. 权威 = attachment 集合；`RenderingInfo` 与 `RenderTargetInfo` 平级

让 `RenderingInfo`（render pass）当权威有两个坑：（1）它混了 attachment 叶子数据与 subpass 拓扑图，
render target 只要前者；（2）dynamic rendering 没有 render pass / subpass，`VkAttachmentDescription`
只存在于 render pass 模型，权威绑在 rendering 上会把耦合请回来。

所以权威是 **attachment 集合本身**（`AttachmentSetInfo`），是 rendering 与 render target 的**共同上游**。

### 2.1 平级，不是 chain —— `begin(cmd, target)` 是证据

决定性证据是 `Rendering::begin(cmd, target)`：target 对应 **framebuffer**，是独立于 render pass 的
运行期对象。基数关系 **1 render pass : N framebuffer**（per-frame-in-flight 就是一个 render pass 配
N 个各绑各 image 的 framebuffer）。若 `RenderingInfo` 拥有/派生 `RenderTargetInfo`，这个 1:N 表达不出来。

二者**平级**，各持同一个 set 的引用：

```
AttachmentSetInfo  ── 权威；build() 冻结「N 个槽 + 每槽角色(color/depth)」，其余字段全空待填
   ├── RenderingInfo(set&)     ── render-scope 半：经 stateRef(i) 填 load/store/layout；另加 subpass 拓扑 + deps
   └── RenderTargetInfo(set&)  ── image-intrinsic 半：经 imageRef(i) 填 format/samples；自持 extent + 绑 image
        二者平级，写 set 里不相交的字段；1 RenderingInfo : N RenderTargetInfo

Rendering::create(device, renderingInfo)                        → VkRenderPass（1 个，读 set 的最终态）
Rendering::begin(cmd, renderTarget)                             → 查/建 framebuffer（N 个，各绑各 image）
Pipeline::create(device, pipelineInfo, rendering, subpassIndex) → pipeline（blend count 取自该 subpass）
```

**subpass 不是平级对象**——它在 render pass 内部。pipeline 靠 `(rendering, subpass_index)` 选取，
无需单独 `SubpassInfo` 传参。set 提供 `colorReference(i, layout)` / `defaultSubpass()` 作为在
`RenderingInfo` 内 author subpass 拓扑的构件；单 subpass 由 `RenderingInfo` 默认带出，多 subpass /
input attachment 依赖则手动 `addSubpass(...)`。

## 3. 冻结范围 = 只冻形态；其余经两个 ref 填

核心简化：`build()` **只冻结 attachment 形态 = 个数 + 每槽角色（color/depth）**。其余字段全部 build 后
经两个 ref 填，各走各的入口、写不相交的字段：

| 数据 | 归类 | 谁 author | build 后可改？ |
|---|---|---|---|
| attachment 个数 + 角色(color/depth) | **形态** | `add*`（build 前） | 冻结 |
| format, samples | image-intrinsic | `RenderTargetInfo::imageRef(i)` | 可改 |
| load/store, stencil ops, initial/final layout, flags | render-scope | `RenderingInfo::stateRef(i)` | 可改 |
| extent, 绑的 image | framebuffer | `RenderTargetInfo` 自持字段 | 可改 |
| resolve mode | subpass 概念 | `AttachmentSetInfo` 平行数组（`add*` 时） | 随形态冻结 |

### 3.1 为什么只冻个数 + 角色

- 两个 ref 都存 `{set&, index}`。index 有效性依赖槽数固定：若 build 后还能 add，已发出的 ref 指向的
  布局会变，新槽还是「format/state 全空」的半成品。冻结个数 = 保证「所有槽都存在，后续只往已存在的槽填」。
- 角色由 `add*` 决定（`addColorAttachment` vs `setDepthStencilAttachment`），它定义 canonical index
  布局 `[color 0..N-1][depth?]`，subpass reference 靠此寻址，故必须与个数一起冻。
- format/samples/layout 不影响布局与 index，无需冻——这是相对 v1 的关键修正（v1 误把 format/samples
  划进冻结层，导致 RenderTargetInfo 不能改 format）。

### 3.2 format 单一权威落在 RenderTargetInfo 侧

RenderTargetInfo 的 `imageRef(i)` 把 format 写进**共享 set**；RenderingInfo 建 `AttachmentDescription`
时从**同一个 set** 读这个 format。format 只被 author 一次（RenderTarget 侧），render pass 与 framebuffer
都从 set 读同一份——§1 的「format 双声明」就此消失，且落点合理：format 本是 image 资源属性。

### 3.3 决策记录：为何 v2 从「快照」改回「共享引用」

v1 让 `RenderingInfo`/`RenderTargetInfo` 各自**快照**一份 set 数据，理由是「调用方不用管 set 生命周期」。
v2 改为**共享同一个 set 引用**，因为那个生命周期顾虑不成立：`RenderingInfo`/`RenderTargetInfo` 都是
**setup 期 schema 对象**，`create()` 时被读一次就烘进 `VkRenderPass` / framebuffer；真正跑 render loop
的是烘出来的 `StaticRendering`（持 VkRenderPass）与 `RenderTarget`（持 framebuffer），二者都不引用 set。
所以 set 只要活过 setup 段，三者同作用域天然满足。引用模型下「单一权威、零 drift」靠**物理共享同一份数据**
达成，比快照 + 约定编辑顺序更直接、更难写错。

### 3.4 不变量

1. **freeze 后禁 `add*`**：`build()` 后 `add*` 返回 error。build 时 `reserve` 到位，杜绝后续 realloc。
   正常流程 `add* → build → (imageRef / stateRef 填) → create`，天然不交叉。
2. **set 必须活过 `create()`**：`RenderingInfo`/`RenderTargetInfo` 持 set 引用，均为 setup 期对象，
   与 set 同作用域。惯例 set 声明在最前。
3. **读前写好**：唯一的时序真实约束——format 要在建 image 前经 imageRef 设好；load/store 要在
   `Rendering::create` 前经 stateRef 设好。两个 ref 写不相交字段，彼此顺序自由。

## 4. 类型定义

骨架如下（省略实现，只定接口与成员布局）。

### 4.1 `AttachmentDescription`（新类，chain 封装 `vk::AttachmentDescription2`）

**新写一个类**（非 `AttachmentStateInfo` 改名），内部 `DynamicStructureChain<vk::AttachmentDescription2>`，
与 `AttachmentReferenceInfo`（封装 `vk::AttachmentReference2`）完全对称。`AttachmentDescription2` 有真实
pNext 需求（如 `vk::AttachmentDescriptionStencilLayout` 分离 depth/stencil layout），chain 值得。命名去
`2` 后缀镜像 vk 结构；命名空间隔离（`lcf::vkc::AttachmentDescription` vs `vk::AttachmentDescription2`）不冲突。

它是 set 内部的元素类型，聚合两类数据（image-intrinsic + render-scope），但**不自己开放 setter 给终端用户**——
写入统一经 `AttachmentSetInfo` 发出的两个 ref（见 4.4 / 4.5），保证权威唯一。

```cpp
class AttachmentDescription
{
    using Self = AttachmentDescription;
    using Root = vk::AttachmentDescription2;
public:
    // rule-of-five;operator const Root &() 返回 chain.root()
    operator const Root &() const noexcept { return m_description.root(); }
    template <utils::struct_extends_c<Root> T>
    T & requestExtension() noexcept { return m_description.template request<T>(); }
    // getters 全量;setter 由 friend 的 ref 类调用(见 4.4/4.5)
    const vk::Format & getFormat() const noexcept;
    const vk::SampleCountFlagBits & getSamples() const noexcept;
    const vk::AttachmentLoadOp & getLoadOp() const noexcept;
    // ... getStoreOp / getStencil* / getInitialLayout / getFinalLayout / getFlags ...
private:
    friend class AttachmentImageRef;   // 写 image-intrinsic 半(format/samples)
    friend class AttachmentStateRef;   // 写 render-scope 半(load/store/layout/flags)
    utils::DynamicStructureChain<Root> m_description;
};
```

### 4.2 `AttachmentSetInfo`（权威容器）

```cpp
class AttachmentSetInfo
{
    using Self = AttachmentSetInfo;
    using ElementList = std::vector<AttachmentDescription>;
    using ResolveModeList = std::vector<vk::ResolveModeFlagBits>;   // 与 m_elements 同 index;depth 项恒 eNone
public:
    ~AttachmentSetInfo() noexcept = default;
    AttachmentSetInfo() = default;
    AttachmentSetInfo(const Self &) = delete;            // 持 ref 期间禁拷贝(不变量)
    AttachmentSetInfo(Self &&) noexcept = default;
    Self & operator=(const Self &) = delete;
    Self & operator=(Self &&) noexcept = default;
public:
    // 形态声明(build 前);只定个数 + 角色,不带 format
    uint32_t addColorAttachment(vk::ResolveModeFlagBits resolve = vk::ResolveModeFlagBits::eNone);  // 返回 index
    uint32_t setDepthStencilAttachment();                                                           // 返回 index
    Self & build() noexcept;   // 冻结「个数 + 角色」+ 填默认 op/layout + reserve(此后 add* 返回 error)

    // subpass 拓扑构件(在 RenderingInfo 内 author 用)
    SubpassDescriptionInfo  defaultSubpass() const;                                    // 全 color + depth,标准 layout
    AttachmentReferenceInfo colorReference(uint32_t index,
        vk::ImageLayout layout = vk::ImageLayout::eColorAttachmentOptimal) const;

    // 查询
    const AttachmentDescription & at(uint32_t index) const noexcept;   // 只读单个元素(取 format 建 image 等)
    uint32_t getAttachmentCount() const noexcept;
    uint32_t getColorAttachmentCount() const noexcept;
    bool     hasDepthStencil() const noexcept;
    uint32_t getDepthStencilIndex() const noexcept;   // 前置:hasDepthStencil()
    vk::ResolveModeFlagBits getResolveMode(uint32_t index) const noexcept;
private:
    friend class RenderingInfo;       // 读全集 desc、发 stateRef
    friend class RenderTargetInfo;    // 读 format/samples、发 imageRef
    friend class AttachmentImageRef;
    friend class AttachmentStateRef;
    ElementList m_elements;
    ResolveModeList m_resolve_modes;
    std::optional<uint32_t> m_depth_stencil_index_opt;
    bool m_built = false;
};
```

### 4.3 `RenderingInfo` 与 `RenderTargetInfo`：各持 set 引用

```cpp
class RenderingInfo   // render-scope 半 + subpass 拓扑
{
    using Self = RenderingInfo;
public:
    explicit RenderingInfo(AttachmentSetInfo & set) noexcept;   // 持引用,不拥有
    AttachmentStateRef stateRef(uint32_t index) noexcept;       // 写 set 元素的 load/store/layout
    Self & addSubpass(SubpassDescriptionInfo subpass);          // 默认已含 set.defaultSubpass()
    Self & addDependency(SubpassDependencyInfo dependency);
    // create() 侧读:全集 AttachmentDescription(从 set)+ subpasses + deps
private:
    AttachmentSetInfo & m_set;
    std::vector<SubpassDescriptionInfo> m_subpasses;
    std::vector<SubpassDependencyInfo> m_dependencies;
    // ... view masks / flags ...
};

class RenderTargetInfo   // image-intrinsic 半 + framebuffer 字段
{
    using Self = RenderTargetInfo;
public:
    explicit RenderTargetInfo(AttachmentSetInfo & set) noexcept;   // 持引用,不拥有
    AttachmentImageRef imageRef(uint32_t index) noexcept;          // 写 set 元素的 format/samples
    Self & setExtent(vk::Extent2D extent) noexcept;
    // extent / 绑 image / clear values 等 framebuffer 字段自持(见 RenderTarget 模型)
private:
    AttachmentSetInfo & m_set;
    vk::Extent2D m_extent;
    // ...
};
```

### 4.4 `AttachmentImageRef`（RenderTargetInfo 发；写 image-intrinsic 半）

```cpp
class AttachmentImageRef
{
    using Self = AttachmentImageRef;
public:
    AttachmentImageRef(AttachmentSetInfo & set, uint32_t index) noexcept;
    Self & setFormat(vk::Format format) noexcept;
    Self & setSamples(vk::SampleCountFlagBits samples) noexcept;
private:
    AttachmentSetInfo & m_set;   // 写回目标,不拥有
    uint32_t m_index;
};
```

### 4.5 `AttachmentStateRef`（RenderingInfo 发；写 render-scope 半）

```cpp
class AttachmentStateRef
{
    using Self = AttachmentStateRef;
public:
    AttachmentStateRef(AttachmentSetInfo & set, uint32_t index) noexcept;
    Self & setLoadStoreOp(vk::AttachmentLoadOp load_op, vk::AttachmentStoreOp store_op) noexcept;
    Self & setStencilLoadStoreOp(vk::AttachmentLoadOp load_op, vk::AttachmentStoreOp store_op) noexcept;
    Self & setLayouts(vk::ImageLayout initial_layout, vk::ImageLayout final_layout) noexcept;
    Self & addFlags(vk::AttachmentDescriptionFlags flags) noexcept;
    // 无 setFormat/setSamples —— 那是 image-intrinsic,归 AttachmentImageRef
private:
    AttachmentSetInfo & m_set;
    uint32_t m_index;
};
```

### 4.6 配套改动

- `StaticRendering::create` **去掉 `RenderTargetInfo` 形参**——format/samples/depth 现都在 set 里，
  `RenderingInfo` 引用同一个 set，create 时读全集 `AttachmentDescription`，不再 zip；硬编码 samples 的
  bug 一并消失。`begin(cmd, target)` 仍收 `RenderTarget`（framebuffer），体现 1 render pass : N framebuffer。
- `StaticGraphicsPipeline::create(device, pipelineInfo, rendering, subpassIndex)`：blend attachment count
  从该 subpass 的 color ref 数**自动推导 / 校验**（默认不透明、写全通道），收掉手传 `ColorBlendStateInfo{count}`。
- 旧 `AttachmentStateInfo` 退役（其字段并入新 `AttachmentDescription`）。
- canonical index 布局 `[color 0..N-1][depth?]`，resolve 目标由 create 侧按 `getResolveMode(i)` 合成、追加。

## 5. 005 example 写法

```cpp
auto [width, height] = window.getPixelSize();

//- 权威:声明形态(个数 + 角色) → build 冻结
vkc::AttachmentSetInfo attachments;
uint32_t color0 = attachments.addColorAttachment();
attachments.build();

//- 两个平级 view,各持 attachments 引用,写不相交字段
vkc::RenderTargetInfo render_target_info(attachments);   // image-intrinsic 半 + framebuffer
render_target_info.imageRef(color0).setFormat(vk::Format::eR8G8B8A8Unorm);
render_target_info.setExtent({width, height});

vkc::RenderingInfo rendering_info(attachments);          // render-scope 半 + subpass(默认单 subpass)
rendering_info.stateRef(color0)
    .setLoadStoreOp(vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore)
    .setLayouts(vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferSrcOptimal);

//- 建 image:format 从 set 单一权威取,不再第二次声明
for (auto & image : render_target_images) {
    vk::ImageCreateInfo image_info;
    image_info.setImageType(vk::ImageType::e2D)
        .setFormat(attachments.at(color0).getFormat())          // 单一权威
        .setExtent({width, height, 1u}).setMipLevels(1u).setArrayLayers(1u)
        .setSamples(vk::SampleCountFlagBits::e1)
        .setUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc);
    // image.create(...) 不变
}
// render_target.build / setColorAttachment 不变(1 render pass : N render target)

vkc::StaticRendering static_rendering;
static_rendering.create(device, rendering_info);   // 读 set 全集 desc,无 zip

//- pipeline:blend count 由 rendering 的 subpass 自动推导,无需手传
vkc::GraphicsPipelineInfo pipeline_info;
pipeline_info.setShaderProgramInfo(std::move(shader_program_info))
    .setViewportStateInfo(viewport_state_info);          // 不再手写 ColorBlendStateInfo{count}
static_graphics_pipeline.create(device, pipeline_info, static_rendering, /*subpass*/0);
```

`eR8G8B8A8Unorm` 只出现一次；render pass attachment desc、render target format、blend count 全从
`attachments` 派生，无第二份数据需手工对齐。两个 view 写 set 里不相交字段，编辑顺序自由；唯一约束是
「读前写好」（format 在建 image 前、load/store 在 create 前）。

## 6. 加深度 = 加数据，不是改流程

```cpp
uint32_t depth = attachments.setDepthStencilAttachment();
attachments.build();
render_target_info.imageRef(depth).setFormat(vk::Format::eD32Sfloat);
rendering_info.stateRef(depth).setLoadStoreOp(vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eDontCare);
// RenderingInfo 默认 subpass 自动含 depthStencil reference;RenderTarget 自动多一个 depth 槽
// blend count 不受影响(depth 非 color)
```

对比当前模型：加深度要同时动 `create` 的 zip 实现、subpass、attachment state 三处并保证对齐。

## 7. dynamic rendering 顺带解锁

`AttachmentSetInfo` 与「render pass vs dynamic」正交：
- **render pass 路径**：`RenderingInfo` 读全集 desc + subpass 引用。
- **dynamic 路径**：subpass 选出的 color 子集 → `PipelineRenderingCreateInfo.colorAttachmentFormats`，
  depth format 同理；load/store 移到 begin 时的 `VkRenderingAttachmentInfo`（与 image view 同居，
  平行数组问题从根消失）。

同一 `attachments` 喂两条路，`StaticGraphicsPipeline` 加 dynamic 重载不用新造 info：

```cpp
std::error_code create(vk::Device, const GraphicsPipelineInfo &, const RenderTargetInfo &) noexcept;
```

内部挂 `vk::PipelineRenderingCreateInfo` 到 pNext，`setRenderPass(nullptr)`。

## 8. 待做项（按依赖顺序）

1. **`AttachmentDescription` 新类**：`DynamicStructureChain<vk::AttachmentDescription2>`，全量 getter，
   `friend` 两个 ref 写入。旧 `AttachmentStateInfo` 退役。
2. **`AttachmentSetInfo` + 两个 ref**：`add*`（只定形态）/ `build`（冻个数+角色）/ `at` getter；
   `AttachmentImageRef`、`AttachmentStateRef`；subpass 构件 `defaultSubpass` / `colorReference`；
   三条不变量（freeze 禁 add + reserve、set 活过 create、读前写好）。
3. **`RenderingInfo` 改造**：改为 `explicit RenderingInfo(AttachmentSetInfo &)` 持引用，暴露 `stateRef`；
   保留 subpass 拓扑 + dependency；缓存 per-subpass color ref 数供 pipeline 推导 blend count。
4. **`RenderTargetInfo` 改造**：`explicit RenderTargetInfo(AttachmentSetInfo &)` 持引用，暴露 `imageRef`；
   保留 extent / 绑 image / clear values（对接 [`vkc-rendertarget-rendering-design.md`](./vkc-rendertarget-rendering-design.md) §2）。
5. **`StaticRendering::create` 去 `RenderTargetInfo` 形参**：全集 desc 从 rendering_info 引用的 set 生成；
   samples 从 desc 取；depth 接入。zip 与硬编码 samples bug 消失。`begin(cmd, target)` 不变。
6. **`StaticGraphicsPipeline::create` blend 自动推导**：从 `(rendering, subpassIndex)` 取 color ref 数，
   补齐 / 校验 `ColorBlendStateInfo`（默认不透明、写全通道）。收掉手传 count。
7. **重写 005 example** 验证（§5）。
8. **（可选）`StaticGraphicsPipeline` dynamic 重载**（§7）+ `DynamicRendering::begin/end`。

**推迟**：depth/stencil resolve；多 subpass 场景的手写 subpass 组装模板化（单 subpass 先行，多 subpass
先用 `addSubpass(...)` + `colorReference(i, layout)` 手动）。
