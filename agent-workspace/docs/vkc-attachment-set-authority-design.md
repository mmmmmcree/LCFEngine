# vkc AttachmentSet + RenderScope 双层设计 (v3)

> 目标：用**同一份描述信息**同时驱动 render pass 与 dynamic rendering 两条路，不隐藏 Vulkan 能力。
> v2 确立了「`AttachmentSetInfo` 是 attachment 池的单一权威」；v3 在其上补一层 **`RenderScopeInfo`**
> ——一次绘制阶段的「选择枢纽」，它是 `VkSubpassDescription2` 与 `VkPipelineRenderingCreateInfo`
> 共同描述面的**并集超集**，向两条路各自投影。
> 本文取代 v2，是对 [`vkc-rendertarget-rendering-design.md`](./vkc-rendertarget-rendering-design.md) §3 的最终演进。
> 正文（§1–§7）给设计与接口骨架；附录 A/B 给 Info 层与 Object 层的完整定义与实现。

---

## 1. 全景：Info 层（描述） + Object 层（GPU 对象）

### 1.1 Info 层（setup 期描述，无 GPU 句柄）

```
AttachmentSetInfo          ── 池权威：N 槽 + 角色 + format/samples/load-store/layout
   │                          canonical 布局 [colors][ds?][resolve]；两条路共享同一批 image
   │
   ├─ RenderTargetInfo(set&)   ── image-intrinsic 半 + 绑定：setFormat(index,fmt) / setSampleCount(range)
   │                             / extent / imageView；1 render : N target
   │
   └─ RenderScopeInfo(set&)    ── 选择枢纽（= 去掉 RenderPass 包袱的 subpass）：
        │                         有序 color 引用 + depth 引用 + input 引用 + resolve 链接 + viewMask
        │                         独立于 VkRenderPass 存在；pipeline 与两条 begin 都消费它
        │
        └─ RenderInfo(set&)     ── 有序 [RenderScopeInfo] + dependency；两种 Render 对象的共同创建源
                                    单 scope 场景默认由一个全覆盖 scope 带出（取代当前 RenderingInfo）
```

### 1.2 Object 层（`create()` 烘出的 GPU 对象）——2 类 Render × 3 类 Pipeline

Vulkan 硬约束决定了 Render 与 Pipeline 都不能是「一个类兼两路」：`renderPass!=null` 的 pipeline 只能在
兼容 render pass 内 bind；`renderPass==null` 的 pipeline 只能在 `vkCmdBeginRendering` 块内 bind。二者无交叉。
故拆开，让「哪种 pipeline 进哪种 render」成为编译期事实。

```
RenderInfo ──create──► 两类 Render 对象（拥有 GPU 句柄；对象内含回填后的 scope）
   │
   ├─ StaticRender    ── VkRenderPass + framebuffer cache。create 后给每个 scope 回填 {VkRenderPass, subpassIndex}
   │                     begin(cmd,target) → vkCmdBeginRenderPass；nextScope → vkCmdNextSubpass；end
   │
   └─ DynamicRender   ── 无 VkRenderPass。begin(cmd,target) 逐 scope 合成 VkRenderingInfo → vkCmdBeginRendering
                         nextScope → EndRendering + 派生 barrier + 下一块 BeginRendering（多 scope，推迟）

Render 对象内含的 scope ──create──► 三类 Pipeline 对象（都「吃一个 scope」，blend-count 恒从 scope color 引用数推）
   │
   ├─ SubpassPipeline        ── 配 StaticRender。scope 携带 {VkRenderPass, subpassIndex}，直接喂
   │                            GraphicsPipelineCreateInfo.renderPass/.subpass；formats 由 RP 隐式定义
   │
   ├─ RenderingPipeline      ── 配 DynamicRender。烘焙式 VkPipeline：renderPass=null +
   │                            VkPipelineRenderingCreateInfo（formats 取自 scope，创建期烘死）
   │
   └─ ShaderObjectPipeline   ── 配 DynamicRender。VK_EXT_shader_object，无 VkPipeline。bind =
                                vkCmdBindShadersEXT + 全套 vkCmdSet*（状态从记录的 scope 现算）
```

使用流（begin / bind* / end，layout 转换归 Render 不归 pipeline）：

```
render.begin(cmd, target);   // 块边界，Render 做 layout 转换（RP 自动 / dynamic 靠 barrier）
pipeline_a.bind(cmd); /* draw */
pipeline_b.bind(cmd); /* draw */    // 同 scope 多 pipeline 共享 attachment，bind 不碰 layout
render.end(cmd);
```

**为什么恰好 3 类不是 4 类**：「dynamic pipeline + render pass」在 Vulkan 不合法（shader object 只配
dynamic rendering），故 `{Subpass}×RP` + `{Rendering, ShaderObject}×dynamic` = 3。

### 1.3 两个不变量

- **为什么 scope 不能当 attachment 数据的权威**：attachment 的身份与 format/samples（即那批共享 image）活在
  scope **之上**——多个 scope 引用同一批 image。若 scope 当权威，同一 attachment 的 format 会在每个引用它的
  scope 里重复，drift 又回来。所以是**两层**：set 定义身份/格式，scope 选择并排布。
- **pipeline 的 formats 永远取自 scope 的 color 选择序列，不是 render 全集**（§2.2）。

## 2. 两条路描述的是同一件事，只是绑定时机不同

pipeline 在两条路里都只需绑定「一个绘制阶段用到哪些 attachment、什么格式、什么顺序」：

- **render pass 路径**：`(VkRenderPass, subpassIndex)` 定位到一个 `VkSubpassDescription2`；format 实际存在
  `VkAttachmentDescription2`，subpass 只存 index+layout。
- **dynamic 路径**：`VkPipelineRenderingCreateInfo{ pColorAttachmentFormats[], depthAttachmentFormat }`
  直接列 format——dynamic 没有 attachment description 这个中间层。

两者都是「按某个顺序引用一批 attachment」，差别只在 format 从哪读。这正是 `RenderScopeInfo` 要抽出的东西。

### 2.2 pipeline 的 formats 恒取自 scope，不是 render 全集（硬约束）

`VkPipelineRenderingCreateInfo.pColorAttachmentFormats` 描述的是**这条 pipeline 实际写出的 color 输出**，
必须逐个、按序匹配 `vkCmdBeginRendering` 的 `pColorAttachments`。render 全集里含 resolve target、以及可能被
别的 scope 用而本 scope 不写的槽——塞全集会出两种错：

- **count 错**：blend attachment count / color output count 必须 = **本 scope 的 color 引用数**，不是 set 槽数。
- **顺序错**：pipeline `location=N` 的输出对应 scope color 序列的第 N 个，不是 set canonical 布局的第 N 个。

所以规则一句话：**pipeline formats = `scope.colorFormats()`（逐个 = `set.at(k).getFormat()`）+ `scope.depthFormat()`**；
resolve target 不进 pipeline formats（它在 begin 侧作为 resolve 目标出现）。三类 pipeline 一致：
`RenderingPipeline`/`ShaderObjectPipeline` 显式用 scope formats 烘焙/现算；`SubpassPipeline` 不显式传 formats
（`{renderPass,subpass}` 隐式定义），但 blend count 同样从**该 scope 的 color 引用数**推导。

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

三个 index 类型独立（color/resolve/ds 各一），惯例上**只应来自 builder 的 `add*` 返回值**。沿此路径两条推论成立：

1. **持有 `DepthStencilAttachmentIndex` ⟹ set 一定有 ds 槽**——该 index 来自
   `enableDepthStencilAttachment()`，它必然置上 ds 标记，`build()` 必然 append ds 槽。
2. **持有 `ResolveAttachmentIndex` ⟹ set 一定有对应 resolve 槽**——来自 `addResolveAttachment()`，
   必然 push 一条 resolve spec，`build()` 必然 append resolve 槽。

结论：**`at(Index)` 返回非 optional `const &`**——「有 index」即「存在性证明」，让已证明存在的调用方再解包
optional 是背叛不变量。optional 只属于「不知道有没有」的 discovery 层（§4.2）。

> 当前实现取裸 `enum class`（可被 brace-init 伪造，靠惯例 + debug 断言兜底）。mint-only（私有构造 +
> `friend` builder）与 provenance（`m_set_id` 盖章、`at()` 校验同源）是**可选加固**，历史方案见 git；
> 接口形态不受影响，需要时替换 index 定义即可。

## 4. 类型定义

### 4.0 三个 index（按角色分型，独立类型互不隐转）

```cpp
enum class ColorAttachmentIndex : uint32_t {};          // canonical 布局 [0, color_count)
enum class ResolveAttachmentIndex : uint32_t {};        // resolve 段内偏移（0 起），非 canonical 下标
enum class DepthStencilAttachmentIndex : uint32_t {};   // 单例，值恒 0；位置归 set（= color_count）
```

跨角色混用（color index 传给 `at(ResolveAttachmentIndex)`）编译期挡掉；到 Vulkan 边界经
`std::to_underlying` 取裸值。

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

    // ── RP 句柄回填（StaticRender::create 后调用；使 scope 成为 SubpassPipeline 的自足创建源）──
    void bindRenderPass(vk::RenderPass rp, uint32_t subpass_index) noexcept;  // friend StaticRender
    bool hasRenderPass() const noexcept { return m_render_pass; }
    vk::RenderPass getRenderPass() const noexcept { return m_render_pass; }   // 前置：hasRenderPass()
    uint32_t getSubpassIndex() const noexcept { return m_subpass_index; }
private:
    friend class StaticRender;
    AttachmentSetInfo & m_set;
    std::vector<AttachmentReferenceInfo> m_color_refs;             // 保留 index+layout
    std::vector<AttachmentReferenceInfo> m_input_refs;
    std::optional<AttachmentReferenceInfo> m_depth_ref;
    uint32_t m_view_mask = 0;
    vk::RenderPass m_render_pass = nullptr;                        // create 后由 StaticRender 回填
    uint32_t m_subpass_index = 0;
    // resolve 链接从 set 的 m_resolve_mode 派生，无需在 scope 重存
};
```

### 4.6 `RenderInfo`（两种 Render 对象的共同创建源，取代当前 `RenderingInfo`）

不再叫 `RenderPassInfo`——它既喂 `StaticRender`（→VkRenderPass）也喂 `DynamicRender`（→无 RP），是路径中立的
描述。dependency 在 RP 路径进 `pDependencies`，在 dynamic 路径退化为块间 barrier（推迟）。

```cpp
class RenderInfo
{
    using Self = RenderInfo;
public:
    explicit RenderInfo(AttachmentSetInfo & set) noexcept;          // 默认带一个全覆盖 defaultScope()
    RenderScopeInfo & defaultScope() noexcept;                      // 单 scope 场景直接用这个
    Self & addScope(RenderScopeInfo scope);                         // 多 scope 手动追加
    Self & addDependency(SubpassDependencyInfo dep);
    std::span<const RenderScopeInfo> getScopes() const noexcept;
    // create 侧读：全集 AttachmentDescription（从 set.allSlots）+ [scope→SubpassDescription2] + deps
    //             preserve attachment 从跨 scope 使用情况派生（推迟，先手写）
private:
    AttachmentSetInfo & m_set;
    std::vector<RenderScopeInfo> m_scopes;
    std::vector<SubpassDependencyInfo> m_dependencies;
    vk::RenderPassCreateFlags m_flags;
};
```

## 4.7 Object 层：2 类 Render + 3 类 Pipeline

Render 对象拥有 GPU 句柄，且**拥有从 `RenderInfo` 移入的 scope 列表**——create 后这些 scope 成为 pipeline
的创建源（`StaticRender` 还会给它们回填 `{VkRenderPass, subpassIndex}`）。

```cpp
class StaticRender      // 对应 VkRenderPass
{
public:
    std::error_code create(vk::Device device, RenderInfo && info) noexcept;
        // 1. 从 info.getScopes() 的 toSubpassDescription() + set.allSlots() 建 VkRenderPass
        // 2. 移入 scope 列表；对每个 scope 调 bindRenderPass(m_render_pass, i)
    const RenderScopeInfo & scopeAt(uint32_t i) const noexcept;     // SubpassPipeline 的创建源
    void begin(vk::CommandBuffer cmd, const RenderTarget & target); // 查/建 framebuffer → vkCmdBeginRenderPass
    void nextScope(vk::CommandBuffer cmd);                          // vkCmdNextSubpass（多 scope）
    void end(vk::CommandBuffer cmd);                                // vkCmdEndRenderPass
private:
    vk::UniqueRenderPass m_render_pass;
    std::vector<RenderScopeInfo> m_scopes;                          // 回填过 RP 句柄
    // framebuffer cache（1 render pass : N target）
};

class DynamicRender     // 对应 vkCmdBeginRendering，无 VkRenderPass
{
public:
    std::error_code create(vk::Device device, RenderInfo && info) noexcept;  // 仅移入 scope，不建 RP
    const RenderScopeInfo & scopeAt(uint32_t i) const noexcept;     // Rendering/ShaderObjectPipeline 的创建源
    void begin(vk::CommandBuffer cmd, const RenderTarget & target); // 逐 scope 合成 VkRenderingInfo + layout barrier
    void nextScope(vk::CommandBuffer cmd);                          // EndRendering + barrier + BeginRendering（推迟）
    void end(vk::CommandBuffer cmd);                                // vkCmdEndRendering
private:
    std::vector<RenderScopeInfo> m_scopes;                          // 无 RP 句柄
};
```

三类 pipeline 都「吃一个 scope」，blend-count 恒从 `scope.colorReferenceCount()` 推。**layout 转换一律归
Render 对象（块边界），pipeline 的 bind 只绑定 + 设动态状态，不碰 layout。**

```cpp
class SubpassPipeline       // 配 StaticRender；renderPass!=null
{
public:
    std::error_code create(vk::Device device, const GraphicsPipelineInfo & info,
        const RenderScopeInfo & scope) noexcept;
        // scope.getRenderPass()/getSubpassIndex() → createInfo.renderPass/.subpass（scope 自足，无需传 render 对象）
        // blend count = scope.colorReferenceCount()；formats 由 RP 隐式定义，不显式传
    void bind(vk::CommandBuffer cmd) const noexcept;                // vkCmdBindPipeline
private:
    vk::UniquePipeline m_pipeline;
};

class RenderingPipeline     // 配 DynamicRender；renderPass=null + 烘焙 formats
{
public:
    std::error_code create(vk::Device device, const GraphicsPipelineInfo & info,
        const RenderScopeInfo & scope) noexcept;
        // VkPipelineRenderingCreateInfo{ colorFormats = scope.colorFormats(), depth = scope.depthFormat() }
        //   挂 pNext，renderPass=null；formats 创建期烘死（§2.2：取 scope 不取 render 全集）
        // 记录 scope 供 bind 时校验「当前块 formats 与烘焙一致」
    void bind(vk::CommandBuffer cmd) const noexcept;
private:
    vk::UniquePipeline m_pipeline;
    const RenderScopeInfo * m_scope;                                // 校验用
};

class ShaderObjectPipeline  // 配 DynamicRender；VK_EXT_shader_object，无 VkPipeline
{
public:
    std::error_code create(vk::Device device, const GraphicsPipelineInfo & info,
        const RenderScopeInfo & scope) noexcept;                    // 建 VkShaderEXT 组，记录 scope
    void bind(vk::CommandBuffer cmd) const noexcept;
        // vkCmdBindShadersEXT + 全套 vkCmdSet*（blend count / color write mask 等从 scope 现算）
private:
    std::vector<vk::UniqueShaderEXT> m_shaders;
    const RenderScopeInfo * m_scope;                                // bind 时现算动态状态
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

### 5.05 共享中段——建 `RenderInfo`（两路同一份）

```cpp
//- RenderInfo 持同一个 set，默认已含全覆盖单 scope；load/store/layout 写在 scope 上
vkc::RenderInfo render_info(attachments);
render_info.defaultScope()
    .addColor(color0, vk::ImageLayout::eColorAttachmentOptimal)
    .setLoadStoreOp(color0, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore)
    .setLayouts(color0, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferSrcOptimal);
```

### 5.1 render pass 路径（StaticRender + SubpassPipeline）

```cpp
//- StaticRender：建 VkRenderPass，并给内含 scope 回填 {VkRenderPass, subpassIndex}
vkc::StaticRender static_render;
static_render.create(device, std::move(render_info));       // 读 set.allSlots + scope→subpass，无 zip

//- SubpassPipeline：吃一个 scope，blend count 自动推导；scope 自带 RP 句柄，无需再传 render 对象
vkc::GraphicsPipelineInfo pipeline_info;
pipeline_info.setShaderProgramInfo(std::move(shader_program_info))
    .setViewportStateInfo(viewport_state_info);
vkc::SubpassPipeline pipeline;
pipeline.create(device, pipeline_info, static_render.scopeAt(0));

//- 循环：Render 做 layout 转换（RP 自动），pipeline.bind 只绑定
static_render.begin(cmd, render_target);        // 查/建 framebuffer → vkCmdBeginRenderPass
pipeline.bind(cmd);
// ... draw ...
static_render.end(cmd);
```

### 5.2 dynamic rendering 路径（DynamicRender + RenderingPipeline）

同一个 `attachments` 与 `RenderInfo`；不建 VkRenderPass，scope 的 formats 喂 pipeline，load/store/layout
移到 begin 的 `VkRenderingAttachmentInfo`（由 DynamicRender 合成）。

```cpp
//- DynamicRender：不建 RP，仅移入 scope
vkc::DynamicRender dynamic_render;
dynamic_render.create(device, std::move(render_info));

//- RenderingPipeline：烘焙式，内部用 scope.colorFormats()/depthFormat() 合成
//  vk::PipelineRenderingCreateInfo 挂 pNext、renderPass=null（formats 取自 scope，§2.2）
vkc::GraphicsPipelineInfo pipeline_info;
pipeline_info.setShaderProgramInfo(std::move(shader_program_info))
    .setViewportStateInfo(viewport_state_info);
vkc::RenderingPipeline pipeline;
pipeline.create(device, pipeline_info, dynamic_render.scopeAt(0));

//- 循环：begin 逐 scope 合成 VkRenderingInfo + layout barrier；pipeline.bind 只绑定
dynamic_render.begin(cmd, render_target);       // → vkCmdBeginRendering(VkRenderingInfo)
pipeline.bind(cmd);
// ... draw ...
dynamic_render.end(cmd);                        // → vkCmdEndRendering
```

> 第三类 `ShaderObjectPipeline` 用法同 5.2，只是 `pipeline.bind(cmd)` 内部是 `vkCmdBindShadersEXT` +
> 全套 `vkCmdSet*`（动态状态从记录的 scope 现算），也配 `DynamicRender`。

**两路差异总结**：前半（set + image + `RenderInfo` 的 scope 选择与 load/store）**完全共享**；分岔只在
①`StaticRender` vs `DynamicRender`；②`SubpassPipeline` vs `RenderingPipeline`（都吃 `render.scopeAt(i)`）；
③循环里 Render 的 begin 实现（framebuffer+vkCmdBeginRenderPass vs VkRenderingInfo）。layout 转换两路都在
Render 的 begin/end，pipeline.bind 一致地只绑定。

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

**Info 层**
1. **`AttachmentSetInfo` 布局改 `[colors][ds?][resolve]`**：`build()` 的 append 顺序确认 ds 在 resolve 之前
   （使 `m_depth_stencil_index == color_count`、resolve 段在尾）。成员 `bool m_has_depth_stencil` 升级为
   `std::optional<uint32_t> m_depth_stencil_index`。
2. **`AttachmentSetInfo` 补接口**：`at(3 类 index)` 非 optional、`getDepthStencilIndex()` 吐 optional key、
   `colorIndex/resolveIndex` discovery、四个 range view（`multisampleSlots/colorSlots/resolveSlots/allSlots`）、
   私有 `mutableAt`。删掉空的 public 段。
3. **删 `AttachmentFormatRef` / `AttachmentStateRef`**：写入改由 `RenderTargetInfo2::setFormat(index,fmt)`
   与 `RenderScopeInfo::setLoadStoreOp/setLayouts(index,...)` 承担，经 friend `mutableAt`。
4. **新建 `RenderScopeInfo`**：选择（color/depth/input/viewMask）+ 状态写回 + 投影
   （`toSubpassDescription` / `colorFormats` / `depthFormat` / `colorReferenceCount`）+ RP 句柄回填
   （`bindRenderPass` / `hasRenderPass` / `getRenderPass` / `getSubpassIndex`，friend `StaticRender`）。
5. **`RenderingInfo` → `RenderInfo`**：持 set 引用、默认 `defaultScope()`、`addScope`/`addDependency`/`getScopes`；
   去掉当前 `AttachmentStateInfo` 平行数组与 zip。
6. **`RenderTargetInfo2` 落地**：`setFormat` 双重载、`setSampleCount` 写 multisample range、extent。

**Object 层（2 Render × 3 Pipeline，替换现 `StaticRendering` / `StaticGraphicsPipeline`）**
7. **`StaticRender`**：`create(device, RenderInfo&&)` 建 VkRenderPass（scope→subpass + `set.allSlots` desc，
   samples 从 desc 取、depth 接入），移入 scope 并逐个 `bindRenderPass`；`scopeAt`；`begin`（framebuffer,
   1 RP : N target）/ `nextScope`（`vkCmdNextSubpass`）/ `end`。
8. **`SubpassPipeline`**：`create(device, pipelineInfo, const RenderScopeInfo&)`——scope 自带 RP 句柄填
   `renderPass/.subpass`，blend count 从 `colorReferenceCount()` 推；`bind`。
9. **`DynamicRender`**：`create(device, RenderInfo&&)` 仅移入 scope；`begin` 逐 scope 合成 `VkRenderingInfo`
   （每 attachment 由 `set.at(k).operator vk::RenderingAttachmentInfo()` + runtime imageView/layout/clear）
   + layout barrier；`nextScope`（EndRendering+barrier+BeginRendering，推迟）/ `end`。
10. **`RenderingPipeline`**：`create(device, pipelineInfo, const RenderScopeInfo&)`——`PipelineRenderingCreateInfo`
    formats 取自 scope（§2.2）、`renderPass=null` 烘焙；记 scope 供 bind 校验；`bind`。
11. **`ShaderObjectPipeline`**（可后置）：`VK_EXT_shader_object`，`bind` = `vkCmdBindShadersEXT` + `vkCmdSet*`
    （动态状态从 scope 现算）。
12. **重写 005 example** 验证两条路（§5）。

**推迟**：depth/stencil resolve（mode 走 pNext）；多 scope 的 dependency 自动派生 + preserve 自动派生
（先手写 `addDependency` / 手工 preserve）；dynamic 多 scope 的 barrier 自动插入（先单 scope）；
`local_read` 下的 input attachment 与 location/input-index 重映射（并集成员已预留，实现推迟）；
`ShaderObjectPipeline`（第 11 项，可延后到 shader object 需求出现时）。

---

## 附录 A：Info 层定义与实现

与 `info_structs.h` 现状对齐（裸 enum index、`AttachmentDescriptionInfo`/builder 已就位）。
相对现状的**唯一数据新增**：set 记录 `m_resolve_sources`（每个 resolve 尾槽的源 color 下标）——
源→target 配对必须存于一处，否则 subpass `pResolveAttachments` 与 dynamic 的 resolveImageView 无从布线。

### A.1 `AttachmentSetInfo`（补全现空壳）

```cpp
class AttachmentSetInfo
{
    friend class AttachmentSetInfoBuilder;
    friend class RenderTargetInfo2;
    friend class RenderScopeInfo;
    using Self = AttachmentSetInfo;
    using DescriptionList = std::vector<AttachmentDescriptionInfo>;
    using ResolveSourceList = std::vector<uint32_t>;   // 与 resolve 尾段同序；值 = 源 color 的 canonical 下标
public:
    ~AttachmentSetInfo() noexcept = default;
    AttachmentSetInfo(const Self &) = delete;
    AttachmentSetInfo(Self &&) noexcept = default;
    Self & operator=(const Self &) = delete;
    Self & operator=(Self &&) noexcept = default;
public:
    //- keyed 访问：非 optional，index 即存在性证明（§3.1）
    const AttachmentDescriptionInfo & at(ColorAttachmentIndex index) const noexcept
    {
        assert(std::to_underlying(index) < m_color_attachment_count);
        return m_descriptions[std::to_underlying(index)];
    }
    const AttachmentDescriptionInfo & at(DepthStencilAttachmentIndex) const noexcept
    {
        assert(m_has_depth_stencil);
        return m_descriptions[m_color_attachment_count];          // 布局固定：ds 恒紧跟 color
    }
    const AttachmentDescriptionInfo & at(ResolveAttachmentIndex index) const noexcept
    {
        assert(std::to_underlying(index) < this->getResolveAttachmentCount());
        return m_descriptions[this->resolveBase() + std::to_underlying(index)];
    }
    //- discovery：optional 包 index（非 desc），回喂 at()
    std::optional<DepthStencilAttachmentIndex> getDepthStencilIndex() const noexcept
    {
        if (not m_has_depth_stencil) { return std::nullopt; }
        return DepthStencilAttachmentIndex {};
    }
    uint32_t getAttachmentCount() const noexcept { return static_cast<uint32_t>(m_descriptions.size()); }
    uint32_t getColorAttachmentCount() const noexcept { return m_color_attachment_count; }
    uint32_t getResolveAttachmentCount() const noexcept { return this->getAttachmentCount() - this->resolveBase(); }
    bool hasDepthStencil() const noexcept { return m_has_depth_stencil; }
    ColorAttachmentIndex colorIndex(uint32_t i) const noexcept
    {
        assert(i < m_color_attachment_count);
        return ColorAttachmentIndex { i };
    }
    ResolveAttachmentIndex resolveIndex(uint32_t i) const noexcept
    {
        assert(i < this->getResolveAttachmentCount());
        return ResolveAttachmentIndex { i };
    }
    //- resolve 配对：给定源 color 的 canonical 下标，查其 target 的 canonical 下标
    std::optional<uint32_t> findResolveTargetSlot(uint32_t color_slot) const noexcept
    {
        for (uint32_t j = 0; j < m_resolve_sources.size(); ++j) {
            if (m_resolve_sources[j] == color_slot) { return this->resolveBase() + j; }
        }
        return std::nullopt;
    }
    //- range view：分区切片
    std::span<const AttachmentDescriptionInfo> colorSlots() const noexcept
    { return { m_descriptions.data(), m_color_attachment_count }; }
    std::span<const AttachmentDescriptionInfo> multisampleSlots() const noexcept   // colors + ds = setSampleCount 作用域
    { return { m_descriptions.data(), this->resolveBase() }; }
    std::span<const AttachmentDescriptionInfo> resolveSlots() const noexcept
    { return { m_descriptions.data() + this->resolveBase(), this->getResolveAttachmentCount() }; }
    std::span<const AttachmentDescriptionInfo> allSlots() const noexcept { return m_descriptions; }
private:
    AttachmentSetInfo() noexcept = default;               // 仅 builder 可造
    uint32_t resolveBase() const noexcept { return m_color_attachment_count + (m_has_depth_stencil ? 1u : 0u); }
    AttachmentDescriptionInfo & mutableAt(uint32_t canonical_slot) noexcept { return m_descriptions[canonical_slot]; }
    std::span<AttachmentDescriptionInfo> mutableMultisampleSlots() noexcept
    { return { m_descriptions.data(), this->resolveBase() }; }
private:
    DescriptionList m_descriptions;
    ResolveSourceList m_resolve_sources;
    uint32_t m_color_attachment_count = 0;
    bool m_has_depth_stencil = false;
};
```

> ds 位置由布局隐含（`= m_color_attachment_count`），故 `bool m_has_depth_stencil` 足够，无需存 index
> ——正文 §3 的 `optional<uint32_t>` 与此等价，取更省的一种。

### A.2 `AttachmentSetInfoBuilder`（现状基础上补 resolve 配对转移）

```cpp
class AttachmentSetInfoBuilder
{
    using Self = AttachmentSetInfoBuilder;
    using DescriptionList = std::vector<AttachmentDescriptionInfo>;
    using ResolveSpecList = std::vector<std::pair<ColorAttachmentIndex, vk::ResolveModeFlagBits>>;
public:
    ColorAttachmentIndex addColorAttachment() noexcept
    {
        m_descriptions.emplace_back();
        return ColorAttachmentIndex { static_cast<uint32_t>(m_descriptions.size() - 1) };
    }
    ResolveAttachmentIndex addResolveAttachment(ColorAttachmentIndex resolved_index,
        vk::ResolveModeFlagBits mode = vk::ResolveModeFlagBits::eAverage) noexcept
    {
        m_resolve_specs.emplace_back(resolved_index, mode);
        return ResolveAttachmentIndex { static_cast<uint32_t>(m_resolve_specs.size() - 1) };
    }
    DepthStencilAttachmentIndex enableDepthStencilAttachment() noexcept
    {
        m_has_depth_stencil = true;
        return DepthStencilAttachmentIndex {};
    }
    AttachmentSetInfo build() noexcept
    {
        AttachmentSetInfo info;
        info.m_color_attachment_count = static_cast<uint32_t>(m_descriptions.size());
        info.m_has_depth_stencil = m_has_depth_stencil;
        m_descriptions.reserve(m_descriptions.size() + m_has_depth_stencil + m_resolve_specs.size());
        if (m_has_depth_stencil) { m_descriptions.emplace_back(); }
        info.m_resolve_sources.reserve(m_resolve_specs.size());
        for (auto [resolved_index, mode] : m_resolve_specs) {
            m_descriptions[std::to_underlying(resolved_index)].setResolveMode(mode);
            m_descriptions.emplace_back().setSampleCount(vk::SampleCountFlagBits::e1);
            info.m_resolve_sources.emplace_back(std::to_underlying(resolved_index));
        }
        info.m_descriptions = std::move(m_descriptions);
        m_descriptions.clear();
        m_resolve_specs.clear();
        m_has_depth_stencil = false;
        return info;
    }
private:
    DescriptionList m_descriptions;
    ResolveSpecList m_resolve_specs;
    bool m_has_depth_stencil = false;
};
```

### A.3 `RenderTargetInfo2`

```cpp
class RenderTargetInfo2
{
    using Self = RenderTargetInfo2;
public:
    ~RenderTargetInfo2() noexcept = default;
    explicit RenderTargetInfo2(AttachmentSetInfo & set) noexcept : m_set(set) {}
    RenderTargetInfo2(const Self &) noexcept = default;
    RenderTargetInfo2(Self &&) noexcept = default;
    Self & operator=(const Self &) = delete;
    Self & operator=(Self &&) = delete;
public:
    Self & setFormat(ColorAttachmentIndex index, vk::Format format) noexcept
    {
        m_set.mutableAt(std::to_underlying(index)).setFormat(format);
        return *this;
    }
    Self & setFormat(DepthStencilAttachmentIndex, vk::Format format) noexcept
    {
        assert(m_set.hasDepthStencil());
        m_set.mutableAt(m_set.m_color_attachment_count).setFormat(format);
        return *this;
    }
    Self & setSampleCount(vk::SampleCountFlagBits samples) noexcept   // 只铺 multisample 前缀，resolve 恒 e1
    {
        m_sample_count = samples;
        for (auto & slot : m_set.mutableMultisampleSlots()) { slot.setSampleCount(samples); }
        return *this;
    }
    Self & setExtent(const vk::Extent2D & extent) noexcept { m_extent = extent; return *this; }
    const vk::Extent2D & getExtent() const noexcept { return m_extent; }
    const vk::SampleCountFlagBits & getSampleCount() const noexcept { return m_sample_count; }
private:
    AttachmentSetInfo & m_set;
    vk::Extent2D m_extent;
    vk::SampleCountFlagBits m_sample_count = vk::SampleCountFlagBits::e1;
};
```

### A.4 `RenderScopeInfo`

```cpp
class RenderScopeInfo
{
    using Self = RenderScopeInfo;
    using ReferenceList = std::vector<AttachmentReferenceInfo>;
public:
    ~RenderScopeInfo() noexcept = default;
    explicit RenderScopeInfo(AttachmentSetInfo & set) noexcept : m_set_p(&set) {}
    RenderScopeInfo(const Self &) = default;              // 指针成员使其可拷贝/移动进 Render 对象
    RenderScopeInfo(Self &&) noexcept = default;
    Self & operator=(const Self &) = default;
    Self & operator=(Self &&) noexcept = default;
public:
    //- 选择（拓扑层，两路都要）
    Self & addColor(ColorAttachmentIndex index,
        vk::ImageLayout layout = vk::ImageLayout::eColorAttachmentOptimal)
    {
        m_color_refs.emplace_back(std::to_underlying(index), layout);
        return *this;
    }
    Self & setDepthStencil(DepthStencilAttachmentIndex,
        vk::ImageLayout layout = vk::ImageLayout::eDepthStencilAttachmentOptimal)
    {
        assert(m_set_p->hasDepthStencil());
        m_depth_ref_opt = AttachmentReferenceInfo { m_set_p->m_color_attachment_count, layout };
        return *this;
    }
    Self & addInput(ColorAttachmentIndex index, vk::ImageLayout layout, vk::ImageAspectFlags aspect_mask)
    {
        m_input_refs.emplace_back(std::to_underlying(index), layout, aspect_mask);
        return *this;
    }
    Self & setViewMask(uint32_t view_mask) noexcept { m_view_mask = view_mask; return *this; }

    //- render-scope 状态写回共享 set（取代 AttachmentStateRef）
    Self & setLoadStoreOp(ColorAttachmentIndex index,
        vk::AttachmentLoadOp load_op, vk::AttachmentStoreOp store_op) noexcept
    {
        m_set_p->mutableAt(std::to_underlying(index)).setLoadStoreOp(load_op, store_op);
        return *this;
    }
    Self & setLoadStoreOp(DepthStencilAttachmentIndex,
        vk::AttachmentLoadOp load_op, vk::AttachmentStoreOp store_op) noexcept
    {
        m_set_p->mutableAt(m_set_p->m_color_attachment_count).setLoadStoreOp(load_op, store_op);
        return *this;
    }
    Self & setInitialFinalLayout(ColorAttachmentIndex index,
        vk::ImageLayout initial_layout, vk::ImageLayout final_layout) noexcept
    {
        m_set_p->mutableAt(std::to_underlying(index)).setInitialFinalLayout(initial_layout, final_layout);
        return *this;
    }
    // ds 版 setInitialFinalLayout / setStencilLoadStoreOp 同构，略

    //- 投影与查询（后端各取所需）
    SubpassDescriptionInfo toSubpassDescription() const    // RP 后端；SubpassDescriptionInfo 拥有 flat 数组
    {
        SubpassDescriptionInfo subpass;
        subpass.setViewMask(m_view_mask);
        for (const auto & ref : m_input_refs) { subpass.addInputAttachment(ref); }
        bool needs_resolve = stdr::any_of(m_color_refs, [this](const auto & ref) {
            return m_set_p->findResolveTargetSlot(ref.getAttachment()).has_value();
        });
        for (const auto & ref : m_color_refs) {
            subpass.addColorAttachment(ref);
            if (not needs_resolve) { continue; }        // pResolveAttachments 要么不给,要么逐 color 给满
            auto target_slot_opt = m_set_p->findResolveTargetSlot(ref.getAttachment());
            subpass.addResolveAttachment(target_slot_opt
                ? AttachmentReferenceInfo { *target_slot_opt, ref.getLayout() }
                : AttachmentReferenceInfo {});           // 默认构造 = eAttachmentUnused
        }
        if (m_depth_ref_opt) { subpass.setDepthStencilAttachment(*m_depth_ref_opt); }
        return subpass;
    }
    std::vector<vk::Format> colorFormats() const           // dynamic 后端，pipeline 侧（§2.2：取 scope 不取全集）
    {
        return m_color_refs | stdv::transform([this](const auto & ref) {
            return m_set_p->allSlots()[ref.getAttachment()].getFormat();
        }) | stdr::to<std::vector>();
    }
    vk::Format depthFormat() const noexcept
    {
        if (not m_depth_ref_opt) { return vk::Format::eUndefined; }
        return m_set_p->allSlots()[m_depth_ref_opt->getAttachment()].getFormat();
    }
    uint32_t colorReferenceCount() const noexcept { return static_cast<uint32_t>(m_color_refs.size()); }
    std::span<const AttachmentReferenceInfo> colorReferences() const noexcept { return m_color_refs; }
    bool hasDepthStencilReference() const noexcept { return m_depth_ref_opt.has_value(); }
    const AttachmentReferenceInfo & getDepthStencilReference() const noexcept { return *m_depth_ref_opt; }
    const uint32_t & getViewMask() const noexcept { return m_view_mask; }
    const AttachmentSetInfo & attachmentSet() const noexcept { return *m_set_p; }

    //- RP 句柄回填（StaticRender::create 后生效；使 scope 成为 SubpassPipeline 的自足创建源）
    bool hasRenderPass() const noexcept { return static_cast<bool>(m_render_pass); }
    vk::RenderPass getRenderPass() const noexcept { return m_render_pass; }     // 前置：hasRenderPass()
    const uint32_t & getSubpassIndex() const noexcept { return m_subpass_index; }
private:
    friend class StaticRender;
    void bindRenderPass(vk::RenderPass render_pass, uint32_t subpass_index) noexcept
    {
        m_render_pass = render_pass;
        m_subpass_index = subpass_index;
    }
private:
    AttachmentSetInfo * m_set_p;
    ReferenceList m_color_refs;
    ReferenceList m_input_refs;
    std::optional<AttachmentReferenceInfo> m_depth_ref_opt;
    uint32_t m_view_mask = 0;
    vk::RenderPass m_render_pass = nullptr;
    uint32_t m_subpass_index = 0;
};
```

### A.5 `RenderInfo`

```cpp
class RenderInfo
{
    using Self = RenderInfo;
    using ScopeList = std::vector<RenderScopeInfo>;
    using DependencyList = std::vector<SubpassDependencyInfo>;
public:
    ~RenderInfo() noexcept = default;
    explicit RenderInfo(AttachmentSetInfo & set) : m_set_p(&set)
    {
        RenderScopeInfo scope(set);                        // 默认全覆盖单 scope：全 color + ds（若有）
        for (uint32_t i = 0; i < set.getColorAttachmentCount(); ++i) { scope.addColor(set.colorIndex(i)); }
        if (auto ds_index_opt = set.getDepthStencilIndex()) { scope.setDepthStencil(*ds_index_opt); }
        m_scopes.emplace_back(std::move(scope));
    }
    RenderInfo(const Self &) = delete;
    RenderInfo(Self &&) noexcept = default;
    Self & operator=(const Self &) = delete;
    Self & operator=(Self &&) noexcept = default;
public:
    RenderScopeInfo & defaultScope() noexcept { return m_scopes.front(); }
    Self & addScope(RenderScopeInfo scope) { m_scopes.emplace_back(std::move(scope)); return *this; }
    Self & addDependency(SubpassDependencyInfo dependency) { m_dependencies.emplace_back(std::move(dependency)); return *this; }
    Self & addFlags(vk::RenderPassCreateFlags flags) noexcept { m_flags |= flags; return *this; }
    std::span<const RenderScopeInfo> getScopes() const noexcept { return m_scopes; }
    std::span<const SubpassDependencyInfo> getDependencies() const noexcept { return m_dependencies; }
    const vk::RenderPassCreateFlags & getFlags() const noexcept { return m_flags; }
    const AttachmentSetInfo & getAttachmentSet() const noexcept { return *m_set_p; }
private:
    friend class StaticRender;    // create 时 move 走 m_scopes
    friend class DynamicRender;
    AttachmentSetInfo * m_set_p;
    ScopeList m_scopes;
    DependencyList m_dependencies;
    vk::RenderPassCreateFlags m_flags {};
};
```

---

## 附录 B：Object 层定义与实现

前置：`CommandBufferProxy` 需补 `beginRendering(const vk::RenderingInfo &)` / `endRendering()` 转发
（dynamic 路径用）。生命周期不变量：**scope 持 set 指针**——`AttachmentSetInfo` 须活过所有 pipeline 的
`create()` 与 `DynamicRender` 的整个使用期（dynamic 的 begin 每帧读 set 的 op/layout）。

### B.1 `StaticRender`（VkRenderPass + framebuffer cache）

```cpp
class StaticRender
{
    using Self = StaticRender;
    using FramebufferCache = std::unordered_map<std::uint64_t, vk::UniqueFramebuffer>;
public:
    std::error_code create(vk::Device device, RenderInfo && render_info) noexcept;
    const RenderScopeInfo & scopeAt(uint32_t index) const noexcept { return m_scopes[index]; }
    void begin(CommandBufferProxy & cmd, const RenderTarget & target) noexcept;   // 查/建 framebuffer
    void nextScope(CommandBufferProxy & cmd) noexcept { cmd.nextSubpass(vk::SubpassContents::eInline); }
    void end(CommandBufferProxy & cmd) noexcept { cmd.endRenderPass(); }
    const vk::RenderPass & getRenderPass() const noexcept { return m_render_pass.get(); }
private:
    vk::Device m_device;
    vk::UniqueRenderPass m_render_pass;
    std::vector<RenderScopeInfo> m_scopes;    // create 后已回填 {VkRenderPass, subpassIndex}
    FramebufferCache m_framebuffer_cache;
};

std::error_code StaticRender::create(vk::Device device, RenderInfo && render_info) noexcept
{
    m_device = device;
    const AttachmentSetInfo & set = render_info.getAttachmentSet();
    auto attachment_descs = set.allSlots()
        | stdv::transform([](const auto & d) -> vk::AttachmentDescription2 { return d; })
        | stdr::to<std::vector>();
    auto subpass_infos = render_info.getScopes()                       // SubpassDescriptionInfo 拥有 flat 数组，
        | stdv::transform([](const auto & s) { return s.toSubpassDescription(); })   // 须存活到 createRenderPass2
        | stdr::to<std::vector>();
    auto subpasses = subpass_infos
        | stdv::transform([](const auto & s) -> vk::SubpassDescription2 { return s; })
        | stdr::to<std::vector>();
    auto dependencies = render_info.getDependencies()
        | stdv::transform([](const auto & d) -> vk::SubpassDependency2 { return d; })
        | stdr::to<std::vector>();
    vk::RenderPassCreateInfo2 render_pass_info;
    render_pass_info.setFlags(render_info.getFlags())
        .setAttachments(attachment_descs)
        .setSubpasses(subpasses)
        .setDependencies(dependencies);
    try {
        m_render_pass = device.createRenderPass2Unique(render_pass_info);
    } catch (const vk::SystemError & e) { return e.code(); }
    m_scopes = std::move(render_info.m_scopes);
    for (uint32_t i = 0; i < m_scopes.size(); ++i) { m_scopes[i].bindRenderPass(m_render_pass.get(), i); }
    return {};
}

void StaticRender::begin(CommandBufferProxy & cmd, const RenderTarget & target) noexcept
{
    // 与现 StaticRendering::begin 相同：以 &target 为 key 查/建 framebuffer（attachments 取
    // target.viewAttachmentImageViews()），随后 cmd.beginRenderPass(..., eInline)。省略。
}
```

### B.2 `SubpassPipeline`（配 StaticRender；scope 自带 RP 句柄）

```cpp
class SubpassPipeline
{
    using Self = SubpassPipeline;
public:
    std::error_code create(vk::Device device, const GraphicsPipelineInfo & pipeline_info,
        const RenderScopeInfo & scope) noexcept;
    void bind(CommandBufferProxy & cmd) const noexcept
    { cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.get()); }
private:
    vk::UniquePipeline m_pipeline;
    vk::UniquePipelineLayout m_pipeline_layout;
    std::vector<vk::UniqueShaderModule> m_shader_modules;
};

std::error_code SubpassPipeline::create(vk::Device device, const GraphicsPipelineInfo & pipeline_info,
    const RenderScopeInfo & scope) noexcept
{
    assert(scope.hasRenderPass());   // 前置：来自 StaticRender::scopeAt()（create 已回填）

    // shader module / pipeline layout 组装与现 StaticGraphicsPipeline::create 相同，略。

    // blend count 自动推导：用户少给的补默认（不透明、写全通道），多给报错
    ColorBlendStateInfo blend_info = pipeline_info.getColorBlendStateInfo();
    if (blend_info.getAttachments().size() > scope.colorReferenceCount()) {
        return make_error_code(errc::color_blend_attachment_count_mismatch);   // errc 需新增
    }
    for (auto i = blend_info.getAttachments().size(); i < scope.colorReferenceCount(); ++i) {
        blend_info.addBlendAttachmentState();
    }

    vk::GraphicsPipelineCreateInfo pipeline_create_info;
    pipeline_create_info /* ... 其余 p*State 同现实现 ... */
        .setPColorBlendState(&static_cast<const vk::PipelineColorBlendStateCreateInfo &>(blend_info))
        .setRenderPass(scope.getRenderPass())              // scope 自足，无需再传 render 对象
        .setSubpass(scope.getSubpassIndex());
    try {
        auto [result, pipeline] = device.createGraphicsPipelineUnique(nullptr, pipeline_create_info);
        if (result != vk::Result::eSuccess) { return result; }
        m_pipeline = std::move(pipeline);
    } catch (const vk::SystemError & e) { return e.code(); }
    return {};
}
```

### B.3 `DynamicRender`（无 VkRenderPass；begin 合成 VkRenderingInfo）

```cpp
class DynamicRender
{
    using Self = DynamicRender;
public:
    std::error_code create(vk::Device device, RenderInfo && render_info) noexcept
    {
        m_device = device;
        m_scopes = std::move(render_info.m_scopes);        // 不建 RP；deps 降解为 barrier（多 scope，推迟）
        return {};
    }
    const RenderScopeInfo & scopeAt(uint32_t index) const noexcept { return m_scopes[index]; }
    void begin(CommandBufferProxy & cmd, const RenderTarget & target) noexcept;
    void nextScope(CommandBufferProxy & cmd) noexcept;     // End + barrier + Begin 下一块（多 scope，推迟）
    void end(CommandBufferProxy & cmd) noexcept { cmd.endRendering(); }
private:
    vk::Device m_device;
    std::vector<RenderScopeInfo> m_scopes;
};

void DynamicRender::begin(CommandBufferProxy & cmd, const RenderTarget & target) noexcept
{
    const RenderScopeInfo & scope = m_scopes.front();      // 单 scope 先行
    const AttachmentSetInfo & set = scope.attachmentSet();

    // layout 转换：dynamic 无隐式转换,块边界发 barrier(各槽 initialLayout → 引用 layout;
    // end 后 final layout 同理)。单 scope 简版按 set 的 initial/final 声明直出,跨帧追踪归 RenderGraph。略。

    std::vector<vk::RenderingAttachmentInfo> color_infos;
    color_infos.reserve(scope.colorReferenceCount());
    for (const auto & ref : scope.colorReferences()) {
        uint32_t slot = ref.getAttachment();               // canonical 下标 = RenderTarget attachment 槽位（1:1）
        vk::RenderingAttachmentInfo info = set.allSlots()[slot];   // authoring 半：loadOp/storeOp/resolveMode
        info.setImageView(target.getAttachment(slot).getImageView())
            .setImageLayout(ref.getLayout())
            .setClearValue(target.getClearValues()[slot]);
        if (auto target_slot_opt = set.findResolveTargetSlot(slot)) {
            info.setResolveImageView(target.getAttachment(*target_slot_opt).getImageView())
                .setResolveImageLayout(vk::ImageLayout::eColorAttachmentOptimal);
        }
        color_infos.emplace_back(info);
    }
    vk::RenderingInfo rendering_info;
    rendering_info.setRenderArea(target.getRenderArea())
        .setLayerCount(target.getLayerCount())
        .setViewMask(scope.getViewMask())
        .setColorAttachments(color_infos);
    vk::RenderingAttachmentInfo depth_info;
    if (scope.hasDepthStencilReference()) {
        const auto & ref = scope.getDepthStencilReference();
        depth_info = set.allSlots()[ref.getAttachment()];
        depth_info.setImageView(target.getAttachment(ref.getAttachment()).getImageView())
            .setImageLayout(ref.getLayout())
            .setClearValue(target.getClearValues()[ref.getAttachment()]);
        rendering_info.setPDepthAttachment(&depth_info);
    }
    cmd.beginRendering(rendering_info);
}
```

### B.4 `RenderingPipeline`（配 DynamicRender；烘焙 formats，renderPass=null）

```cpp
class RenderingPipeline
{
    using Self = RenderingPipeline;
public:
    std::error_code create(vk::Device device, const GraphicsPipelineInfo & pipeline_info,
        const RenderScopeInfo & scope) noexcept;
    void bind(CommandBufferProxy & cmd) const noexcept
    { cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.get()); }
private:
    vk::UniquePipeline m_pipeline;
    vk::UniquePipelineLayout m_pipeline_layout;
    std::vector<vk::UniqueShaderModule> m_shader_modules;
    const RenderScopeInfo * m_scope_p = nullptr;           // bind 期校验用（debug）
};

std::error_code RenderingPipeline::create(vk::Device device, const GraphicsPipelineInfo & pipeline_info,
    const RenderScopeInfo & scope) noexcept
{
    // shader module / layout / blend 补齐与 SubpassPipeline 相同，略。

    auto color_formats = scope.colorFormats();             // §2.2：formats 恒取 scope，不取 render 全集
    vk::PipelineRenderingCreateInfo rendering_create_info;
    rendering_create_info.setViewMask(scope.getViewMask())
        .setColorAttachmentFormats(color_formats)
        .setDepthAttachmentFormat(scope.depthFormat());
        // stencil：若 depth format 含 stencil aspect 一并 setStencilAttachmentFormat，略

    vk::GraphicsPipelineCreateInfo pipeline_create_info;
    pipeline_create_info /* ... 其余 p*State 同上 ... */
        .setPNext(&rendering_create_info)
        .setRenderPass(nullptr);                           // dynamic 路径标志
    try {
        auto [result, pipeline] = device.createGraphicsPipelineUnique(nullptr, pipeline_create_info);
        if (result != vk::Result::eSuccess) { return result; }
        m_pipeline = std::move(pipeline);
    } catch (const vk::SystemError & e) { return e.code(); }
    m_scope_p = &scope;
    return {};
}
```

### B.5 `ShaderObjectPipeline`（配 DynamicRender；VK_EXT_shader_object，推迟）

```cpp
class ShaderObjectPipeline
{
    using Self = ShaderObjectPipeline;
public:
    std::error_code create(vk::Device device, const GraphicsPipelineInfo & pipeline_info,
        const RenderScopeInfo & scope) noexcept;           // vkCreateShadersEXT 建 VkShaderEXT 组，记录 scope
    void bind(CommandBufferProxy & cmd) const noexcept;
        // vkCmdBindShadersEXT + 全套 vkCmdSet*：
        //   blend count / color write mask ← m_scope_p->colorReferenceCount()
        //   rasterizationSamples           ← m_scope_p->attachmentSet().multisampleSlots() 的 samples
        //   viewport/scissor/vertex input  ← pipeline_info 记录的状态
private:
    std::vector<vk::UniqueShaderEXT> m_shaders;
    const RenderScopeInfo * m_scope_p = nullptr;           // bind 期现算动态状态
};
```

实现推迟至 shader object 需求出现（§7 第 11 项）；接口定型即可，与前两类同构（create 吃 scope、bind 无参）。



