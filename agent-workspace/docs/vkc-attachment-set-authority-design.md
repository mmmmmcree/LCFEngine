# vkc Render 抽象设计 (v4)

> **约束**：vkc 不遮蔽 Vulkan 本身的能力，同时给出符合用户逻辑的封装。
> v4 相对 v3 的核心修正：**抽象不能发生在 authoring 期**。v3 造了一个 `RenderScopeInfo` 让用户去填，
> 用它「同时投影两条路」——这是把两条路强行压成一个不存在的公共 authoring 面。v4 的立场：
>
> - **authoring 面按路分开，每个类 1:1 贴一个 vk struct**（`StaticRenderInfo` = `VkRenderPassCreateInfo2`，
>   直接吃已有的 `SubpassDescriptionInfo`；`DynamicRenderInfo` = `VkRenderingInfo` 的选择面）。
> - **render scope 降级为 Render 对象 `create()` 之后吐出的只读信息**——它不是用户填的描述，
>   是「已经存在的 render 块对 pipeline 承诺的兼容性」。两条路各自一个类型
>   （`StaticRenderScopeInfo` / `DynamicRenderScopeInfo`），不强求同一类。
> - 两条路真正共享的只有 **attachment 表本身**（`AttachmentSetInfo`），这一点由 §2 的字段级对照证明，
>   不是拍出来的。
>
> 本文取代 v3。§2 是全文的地基：**先对照 vk struct，再决定抽象什么**。

---

## 1. v3 的三处过早抽象

| v3 做法 | 问题 |
|---|---|
| 用户填一个 `RenderScopeInfo`，它是 `VkSubpassDescription2` ∪ `VkPipelineRenderingCreateInfo` 的「并集超集」 | 两个 struct 处在**不同生命周期**（一个是 RP 烘焙期描述，一个是 pipeline 创建期描述），并集是人造物，用户填的时候无法知道自己在填哪一路的哪一半 |
| `RenderInfo` 持有 `[RenderScopeInfo]`，两条路共用 | dynamic rendering **没有 subpass 数组**。让 dynamic 路吃一个「scope 列表」等于替 Vulkan 发明了它没有的结构，且逼 vkc 去合成它无法正确合成的 barrier |
| 3 类 pipeline（`SubpassPipeline` / `RenderingPipeline` / `ShaderObjectPipeline`） | 前两者产物与 bind 命令完全相同，差别只是**同一个 `VkGraphicsPipelineCreateInfo` 填哪个字段**；`ShaderObjectPipeline` 更是错位——shader object 是 stage 粒度、覆盖 compute/mesh，**创建期与 render scope 零耦合** |

共同病根：**在还不知道两条路哪些字段真的重合时就先造了一个中间类型**。所以先做对照。

## 2. 字段级对照：静态路径 vs 动态路径

### 2.1 静态路径涉及的 vk struct

```c
VkRenderPassCreateInfo2 {                    // 烘焙期，一次
    VkRenderPassCreateFlags         flags;
    uint32_t                        attachmentCount;
    const VkAttachmentDescription2* pAttachments;        // ← attachment 表（身份 + 格式 + 状态）
    uint32_t                        subpassCount;
    const VkSubpassDescription2*    pSubpasses;          // ← 多阶段编排
    uint32_t                        dependencyCount;
    const VkSubpassDependency2*     pDependencies;       // ← 阶段间同步（驱动实现）
    uint32_t                        correlatedViewMaskCount;
    const uint32_t*                 pCorrelatedViewMasks;
}
VkAttachmentDescription2 {  flags; format; samples;
    loadOp; storeOp; stencilLoadOp; stencilStoreOp;
    initialLayout; finalLayout; }                        // ← 块进入/退出 layout，RP 自动转换
VkSubpassDescription2 { flags; pipelineBindPoint; viewMask;
    inputAttachmentCount; pInputAttachments;             // ← 原生 input attachment
    colorAttachmentCount; pColorAttachments;             // ← 有序 color 选择（AttachmentReference2）
    pResolveAttachments;                                 // ← 与 pColorAttachments 逐个对位
    pDepthStencilAttachment;                             // ← depth+stencil 合一
    preserveAttachmentCount; pPreserveAttachments; }
VkAttachmentReference2 { attachment; layout; aspectMask; }   // 只有 index+layout，无 format

VkFramebufferCreateInfo { renderPass; pAttachments /*VkImageView[]*/; width; height; layers; }
VkRenderPassBeginInfo   { renderPass; framebuffer; renderArea; pClearValues; }   // clear 按 attachment 下标
```

### 2.2 动态路径涉及的 vk struct

```c
VkRenderingInfo {                            // 每帧/每块，record 期
    VkRenderingFlags                 flags;
    VkRect2D                         renderArea;
    uint32_t                         layerCount;
    uint32_t                         viewMask;
    uint32_t                         colorAttachmentCount;
    const VkRenderingAttachmentInfo* pColorAttachments;  // ← 有序 color 选择，内联全部信息
    const VkRenderingAttachmentInfo* pDepthAttachment;   // ← depth 与 stencil 分开
    const VkRenderingAttachmentInfo* pStencilAttachment;
}
VkRenderingAttachmentInfo { imageView; imageLayout;      // ← 单个 layout，无 initial/final
    resolveMode; resolveImageView; resolveImageLayout;   // ← resolve 挂在源 attachment 上
    loadOp; storeOp; clearValue; }                       // ← load/store/clear 内联，无 stencil* 字段

VkPipelineRenderingCreateInfo {              // pipeline 创建期（pNext 挂 GraphicsPipelineCreateInfo）
    uint32_t viewMask;
    uint32_t colorAttachmentCount; const VkFormat* pColorAttachmentFormats;   // ← format 在这里
    VkFormat depthAttachmentFormat; VkFormat stencilAttachmentFormat; }
// VK 1.4 core（local_read）：块内重映射，base dynamic 没有
VkRenderingAttachmentLocationInfo   { colorAttachmentCount; pColorAttachmentLocations; }
VkRenderingInputAttachmentIndexInfo { colorAttachmentCount; pColorAttachmentInputIndices;
                                      pDepthInputAttachmentIndex; pStencilInputAttachmentIndex; }
```

### 2.3 逐关注点对位

| 关注点 | 静态路径 | 动态路径 | 结论 |
|---|---|---|---|
| attachment 身份/**format** | `VkAttachmentDescription2.format`（表，烘焙期） | pipeline 侧 `pColorAttachmentFormats` + begin 侧 `imageView` 隐含 | **值相同，落点不同** → 值可共享 |
| **samples** | `VkAttachmentDescription2.samples` | 无字段；由 imageView 的 image 决定，须等于 pipeline 的 `rasterizationSamples` | 值相同，动态路径无处声明 → 由共享表兜底 |
| **loadOp/storeOp** | `VkAttachmentDescription2`（表，烘焙期定死） | `VkRenderingAttachmentInfo`（每块可变） | 值相同，**时机不同** |
| stencil load/store | `stencilLoadOp/stencilStoreOp` 同一条 desc | 独立的 `pStencilAttachment` 自带 loadOp/storeOp | 结构不同：合一 vs 分离 |
| **layout** | `initialLayout` / `finalLayout`（块边界）+ ref 的 `layout`（块内） | 只有 `imageLayout`（块内）；进出转换须自己发 barrier | **动态路径少两个字段**——initial/final 是静态路径独有 |
| 有序 color 选择 | `pColorAttachments`：`AttachmentReference2{index, layout}` | `pColorAttachments`：`RenderingAttachmentInfo{view, layout, ops...}` | **同一语义（有序引用），载体完全不同** |
| resolve | 独立 `pResolveAttachments`，与 color 逐个对位 | `resolveMode/resolveImageView` 挂在源 color 上 | 语义相同，结构不同 |
| depth/stencil | 一个 `pDepthStencilAttachment` | 两个指针，可指向不同 view | 结构不同 |
| input attachment | 原生 `pInputAttachments` | base 无；需 `local_read`（VK 1.4）+ 两个重映射 struct | **动态路径能力缺口** |
| 多阶段编排 | `pSubpasses[]` + `pDependencies[]`，一次 begin/end | N 个 begin/end 块 + 手写 barrier | **动态路径没有对应结构** |
| clearValue | `RenderPassBeginInfo.pClearValues[]`，按表下标 | 每个 `RenderingAttachmentInfo.clearValue` | 值相同，落点不同 |
| imageView | 烘进 `VkFramebuffer`（对象，可缓存） | 每次 begin 直接给 | 值相同，落点不同 |
| viewMask | `VkSubpassDescription2.viewMask` | `VkRenderingInfo.viewMask`（须 == pipeline 的） | 值相同 |
| renderArea | `RenderPassBeginInfo.renderArea` | `RenderingInfo.renderArea` | 值相同 |
| layerCount | `VkFramebufferCreateInfo.layers` | `VkRenderingInfo.layerCount` | 值相同 |
| **pipeline 侧兼容性** | `.renderPass` + `.subpass`（format 由 RP 隐含） | `.renderPass = NULL` + pNext `VkPipelineRenderingCreateInfo`（显式 format） | **同一个 `VkGraphicsPipelineCreateInfo` 的两组字段** |

### 2.4 从对照表读出的三条结论

1. **能共享的只有「每个 attachment 的一组值」**：format、samples、load/store、clear、imageView、resolveMode、
   以及各自的 layout 意图。这些值两条路都要，只是**落点与时机不同**。
   → 这正当化 `AttachmentSetInfo` 作为**表权威**，且它是唯一的共享物。
2. **不能共享的是「编排结构」**：静态路径是 `pSubpasses[] + pDependencies[]` 的一次性图；动态路径是
   N 个块 + 手写 barrier。动态路径**根本没有 subpass 数组这个 slot**。
   → 任何「统一的 scope 列表」都是在替 Vulkan 发明结构。v3 的 `RenderInfo` 就栽在这。
3. **两条路唯一真正汇聚的地方是 pipeline 创建**——`VkGraphicsPipelineCreateInfo` 用
   `{renderPass, subpass}` 或 `{NULL, pNext:PipelineRenderingCreateInfo}` 表达同一件事：
   「这条 pipeline 兼容什么 render 块」。注意汇聚点是**那一个 create info**，
   而两组字段本身**不相交**。
   → 所以 render scope 该存在的位置是：**Render 对象建好后吐出的兼容性凭据**（而非用户填的描述），
   且**两条路各一个类型**——共用的是消费它的 `GraphicsPipeline`，不是 scope 自己。

## 3. 全景：谁在什么时候存在

```
── authoring 期（无 GPU 句柄，每类 1:1 贴一个 vk struct）───────────────────────
AttachmentSetInfo        唯一共享物：N 槽 × {format, samples, load/store, layouts, resolveMode}
   │                     canonical 布局 [colors][ds?][resolve]（§4）
   ├── RenderTargetInfo2(set&)   写 image-intrinsic 半：setFormat(index) / setSampleCount / extent
   │
   ├── StaticRenderInfo(set&)    ≡ VkRenderPassCreateInfo2
   │      直接吃已有的 SubpassDescriptionInfo（≡ VkSubpassDescription2）+ SubpassDependencyInfo
   │      attachment 表从 set 取，用户不重复填
   │
   └── DynamicRenderInfo(set&)   ≡ 一个 VkRenderingInfo 块的「选择面」
          有序 color 引用 + depth 引用 + stencil 引用 + viewMask + flags
          （无 subpass 数组、无 dependency——Vulkan 这条路没有这两个 slot）

── create() 之后（GPU 对象 + 只读凭据）──────────────────────────────────────
StaticRenderInfo  ──create──►  StaticRender    { VkRenderPass, framebuffer cache }
                                  └─ scopeAt(i) ──► const StaticRenderScopeInfo &
                                                    { renderPass, subpass, colorCount, samples }
DynamicRenderInfo ──create──►  DynamicRender   { 无 GPU 句柄，预算好的 begin 材料 }
                                  └─ getScope() ──► const DynamicRenderScopeInfo &
                                                    { colorFormats[], depth/stencilFormat, viewMask, samples }

── 消费 scope ───────────────────────────────────────────────────────────────
GraphicsPipeline::create(device, info, const StaticRenderScopeInfo &)    // → .renderPass/.subpass
GraphicsPipeline::create(device, info, const DynamicRenderScopeInfo &)   // → renderPass=null + pNext
ShaderObjectGroup::create(device, stages, layouts)      无 scope —— 创建期与 render 零耦合
ShaderObjectGroup::bind(cmd, const DynamicRenderScopeInfo &)   scope 在 bind 期现算动态状态
```

**两个 scope 都是只读凭据，不是 authoring 类型**——没有 public 构造，只能从对应的 Render 对象拿到。
「持有一个 scope」⟹「对应的 render 块已经存在且已建好」。这是 v4 与 v3 最实质的差别。
**不强行合成一个类**：两者字段不相交（一个是句柄+下标，一个是 format 数组），
合并只会得到一个带 tag 的 variant + 一半死字段；分开则每个 1:1 贴一个 vk struct，
且「哪种 pipeline 进哪种 render」由**重载决议**在编译期定死。

### 3.1 使用流

```cpp
render.begin(cmd, target);      // 块边界：layout 转换归 Render（RP 自动 / dynamic 由上层 barrier）
pipeline_a.bind(cmd);  /* draw */
pipeline_b.bind(cmd);  /* draw */   // 同块多 pipeline 共享 attachment，bind 不碰 layout
render.end(cmd);
```

## 4. `AttachmentSetInfo`：唯一共享物

### 4.1 三个角色的基数不同，不该用同一种「下标」表达

v3 把三种角色塞进一个 `vector` 并用 `bool m_has_depth_stencil` 做下标算术
（`resolveBase() = color_count + has_ds`、`at(ds) → m_descriptions[color_count]`）。别扭点在于
**三者的基数与身份来源根本不同**：

| 角色 | 基数 | 身份是什么 | 有独立的 format/samples 吗 |
|---|---|---|---|
| color | 0..N | **序号**（location 语义，有序） | 有 |
| depth/stencil | **0 或 1** | **存在性本身**——没有序号可言 | 有 |
| resolve | 0..M（M ≤ N） | **它配对的那个 color**——不是自己的序号 | **没有**：Vulkan 要求与源 color 同 format，samples 恒 `e1` |

两处建模错误：

1. **ds 用 `enum class : uint32_t`**——值域只有一个元素，却带一个恒为 0 的 `uint32_t`。
   这也是 v3 里 `at(DepthStencilAttachmentIndex index)` 形参 `index` 从未被使用的原因。
   「index」这个词本身就不成立：**它不索引任何东西，它只证明「有」**。
2. **resolve 有自己的 index**——`ResolveAttachmentIndex{0}` 是「第 0 个 resolve」，
   但 Vulkan 两条路**都不用这个编号寻址**：静态路径 `pResolveAttachments[i]` 对位
   `pColorAttachments[i]`；动态路径 `resolveImageView` 直接挂在源 color 的
   `VkRenderingAttachmentInfo` 上。**两条路都经 color 寻址 resolve**。
   给用户一个 Vulkan 自己不用的编号，是发明寻址方式。

### 4.1.1 修正：Key 不是 Index，且 resolve 不需要自己的 Key

- **改名 Index → Key**：这些东西的作用是「凭它取到某个 attachment」，不是「它是第几个」。
  color 的 key 内部确实带序号（location 语义要求），ds 的 key 不带任何值——
  统一叫 Key，语义才对得上。
- **删掉 `ResolveAttachmentKey`**：resolve 经**源 color 的 key** 寻址。
  这既贴 Vulkan（上表末行），也消掉一个类型。于是全局只剩 **2 类 key**。

```cpp
//- authoring：resolve 是 color 的一个属性，不是第三种 attachment 角色
ColorAttachmentKey color0 = builder.addColorAttachment();
builder.enableResolve(color0, vk::ResolveModeFlagBits::eAverage);   // 无返回值——不需要新 key

//- 消费：一律经 color0
set.hasResolve(color0);                     // 有没有 resolve target
set.at(color0).getResolveMode();
render_target.setResolveAttachment(color0, resolve_image);   // 绑 imageView
static_render_info.setResolveLoadStoreOp(color0, eDontCare, eStore);
```

### 4.2 方案：按角色分开存，canonical 布局只在**落地时**才存在

存储按角色分三份，各自基数自然；把三者拼成一条 flat 数组是**静态路径的落地需求**，
不是 authoring 期的表达方式。

```cpp
private:
    std::vector<AttachmentDescriptionInfo> m_color_descriptions;      // 基数 0..N
    std::vector<AttachmentDescriptionInfo> m_resolve_descriptions;    // 基数 0..M
    std::vector<uint32_t> m_resolve_sources;      // 与上者同序，值 = 源 color 的序号
    std::vector<std::optional<uint32_t>> m_color_to_resolve;   // 与 color 同序，反查（O(1)）
    std::optional<AttachmentDescriptionInfo> m_depth_stencil_description_opt;   // 基数 0..1
```

`m_color_to_resolve` 是 §4.1.1 的直接后果：既然一律经 color 寻址 resolve，反查必须 O(1)，
不能像 v3 那样线性扫 `m_resolve_sources`。两份配对信息互为逆映射，都在 `build()` 里一次填好。

### 4.2 方案：按角色分开存，canonical 布局只在**落地时**才存在

存储按角色分三份，各自基数自然；把三者拼成一条 flat 数组是**静态路径的落地需求**，
不是 authoring 期的表达方式。

```cpp
private:
    std::vector<AttachmentDescriptionInfo> m_color_descriptions;      // 基数 0..N
    std::vector<AttachmentDescriptionInfo> m_resolve_descriptions;    // 基数 0..M
    std::optional<AttachmentDescriptionInfo> m_depth_stencil_description_opt;   // 基数 0..1
```

canonical 布局（仅 `VkRenderPass` / framebuffer 需要）：

```
flat 下标:  0 .. C-1        C .. C+M-1         C+M (若有)
           [ color 0..C-1 | resolve 0..M-1 | depth_stencil? ]
                            └── 恒 e1 ──┘     └─ 单例落尾 ─┘
```

`slotOf` 全部**无条件、无 `has_ds` 参与**：

```cpp
uint32_t slotOf(ColorAttachmentKey k) const noexcept { return k.ordinal(); }
uint32_t slotOf(DepthStencilAttachmentKey) const noexcept { return colorCount() + resolveCount(); }
uint32_t resolveSlotOf(ColorAttachmentKey k) const noexcept   // 前置 hasResolve(k)
{ return colorCount() + *m_color_to_resolve[k.ordinal()]; }
```

`slotOf(ds)` 里的 `+ resolveCount()` 不是「条件算术」——它是 ds 落在尾部这一事实的直接表达，
且**不因 ds 是否存在而改变其他角色的下标**。对比 v3：ds 在中间时，`resolveBase()` 必须问
「ds 在不在」，于是 `has_ds` 渗进每一处 resolve 寻址。

**为什么 ds 落尾而不是居中**：v3 让 ds 紧跟 color，理由是「让多重采样槽成为连续前缀，
`setSampleCount` 一步写完」。但这是**为了一个 setter 的实现方便，扭曲了整个布局的语义**。
按角色分存后，`setSampleCount` 本来就该分别写两处：

```cpp
Self & setSampleCount(vk::SampleCountFlagBits samples) noexcept   // RenderTargetInfo2
{
    for (auto & slot : m_set.mutableColorDescriptions()) { slot.setSampleCount(samples); }
    if (auto * ds_p = m_set.mutableDepthStencilDescription()) { ds_p->setSampleCount(samples); }
    return *this;                        // resolve 段根本不在视野内，天然恒 e1
}
```

两句话，且**不依赖任何布局假设**——resolve 不是「被 range 跳过」的，它压根不在遍历范围里。
这比「靠布局技巧让一个 range 恰好覆盖对的槽」更难写错。

顺带的收益：
- **flat 数组只在 `StaticRenderInfo` 落地时拼**（`flattenDescriptions()`），动态路径完全不需要它。
  动态路径本就按角色分开取（`pColorAttachments` / `pDepthAttachment` / `pStencilAttachment`），
  §2.3 那一行「结构不同：合一 vs 分离」在这里得到回报。
- resolve 配对存**双向**（`m_resolve_sources` + `m_color_to_resolve`），因为 §4.1.1 之后
  一律经 color 反查，O(n) 线性扫不可接受。
- 未来 depth/stencil resolve（`VkSubpassDescriptionDepthStencilResolve`）追加为第四份存储 +
  落在 flat 尾部，**不改动任何既有下标**。v3 的居中布局做这件事要重排。

> **canonical 下标 = framebuffer / RenderTarget 的 attachment 槽位**（1:1）。这个契约由
> `slotOf` 独家定义——`RenderTarget` 供 imageView 时按同一函数排序，静态路径的
> `AttachmentReference2.attachment` 与动态路径取 view 都过它。布局知识只此一处。

### 4.3 Key：mint-only + provenance，把「不变量」升级成「保证」

v3 用裸 `enum class`，靠**惯例**维持「key 只来自 builder」——但 `ColorAttachmentIndex{7}` 是合法的
brace-init，`DepthStencilAttachmentIndex{}` 更是随手可造。于是「持有 ds key ⟹ set 有 ds 槽」
只是注释里的承诺，编译器一无所知。两个 builder 的 key 混用（builder1 的 key 喂 builder2 的 set）
也毫无阻挡。

修正：**key 只能由 builder 铸造**（私有构造 + friend），且**盖 set id 章**。

```cpp
enum class AttachmentSetId : uint64_t { eInvalid = 0 };
AttachmentSetId next_attachment_set_id() noexcept;      // 单调递增，跨 builder 唯一

//- 有序号角色（目前只有 color）
template <typename Role>
class OrdinalAttachmentKey
{
    friend class AttachmentSetInfoBuilder;
public:
    OrdinalAttachmentKey() = delete;                    // 无默认构造：不存在「空 key」
    uint32_t ordinal() const noexcept { return m_ordinal; }
    AttachmentSetId setId() const noexcept { return m_set_id; }
private:
    OrdinalAttachmentKey(uint32_t ordinal, AttachmentSetId set_id) noexcept
        : m_ordinal(ordinal), m_set_id(set_id) {}
    uint32_t m_ordinal;
    AttachmentSetId m_set_id;
};

//- 单例角色（目前只有 depth/stencil）：无序号，只有 provenance
template <typename Role>
class SingletonAttachmentKey
{
    friend class AttachmentSetInfoBuilder;
public:
    SingletonAttachmentKey() = delete;
    AttachmentSetId setId() const noexcept { return m_set_id; }
private:
    explicit SingletonAttachmentKey(AttachmentSetId set_id) noexcept : m_set_id(set_id) {}
    AttachmentSetId m_set_id;
};

struct ColorAttachmentRole {};
struct DepthStencilAttachmentRole {};
using ColorAttachmentKey        = OrdinalAttachmentKey<ColorAttachmentRole>;
using DepthStencilAttachmentKey = SingletonAttachmentKey<DepthStencilAttachmentRole>;
```

三条私有化换来的东西：

| 手段 | 挡住的错误 |
|---|---|
| 私有构造 + `friend builder` | `ColorAttachmentKey{7}` / `DepthStencilAttachmentKey{}` 伪造——**编译期** |
| `= delete` 默认构造 | 「先声明一个空 key 再赋值」这类绕过 mint 的路径——**编译期** |
| `m_set_id` + `at()` 校验 | builder1 的 key 喂 builder2 的 set——**运行期**（`assert` / 返回 errc） |
| 独立 Role 类型 | color key 传进 `at(DepthStencilAttachmentKey)`——**编译期** |

**于是 §4.1 的目标达成**：`enableDepthStencilAttachment()` 是 `DepthStencilAttachmentKey` 的
**唯一**来源，它必然置上 ds 存在标记。所以

```
持有 DepthStencilAttachmentKey  ⟹  某个 builder 上调用过 enableDepthStencilAttachment()
                               ⟹  它 build() 出的 set 一定有 ds 槽
+ set_id 校验通过              ⟹  就是这个 set
```

**没有 ds 就拿不到 ds key，拿不到 key 就写不出访问 ds 的代码**——不是断言兜底，是构造不出来。
`at(Key)` 因此返回非 optional `const &`：optional 只属于「不知道有没有」的 discovery 层
（`getDepthStencilKey()` 返回 `optional<DepthStencilAttachmentKey>`，供泛型代码回喂 `at()`）。

> **set_id 为何只能是运行期**：把 id 提升为模板参数才能编译期校验，但那要求 id 是常量表达式，
> 于是 set 的类型随 builder 实例变化——`AttachmentSetInfo` 不再是单一类型，无法放进容器、
> 无法作函数参数。代价远超收益。**跨 set 混用是罕见错误，跨角色混用与伪造是常见错误**——
> 后两者已在编译期挡住，前者用 `assert` + release 下的 errc 是正确的性价比点。
>
> **builder 复用**：`build()` 后 builder 轮换到新的 `m_batch_id`，所以第二次 `build()` 的 set
> 与第一批 key 的 id 不同——旧 key 访问新 set 会被校验挡下，而不是静默错位。

### 4.4 接口

```cpp
class AttachmentSetInfo
{
    friend class AttachmentSetInfoBuilder;
    friend class RenderTargetInfo2;
    friend class StaticRenderInfo;
    friend class DynamicRenderInfo;
public:
    AttachmentSetInfo(const Self &) = delete;            // 持 ref 期间禁拷贝
    AttachmentSetInfo(Self &&) noexcept = default;

    //- keyed 访问：非 optional，key 即存在性证明（§4.3）；内部先校验 setId
    const AttachmentDescriptionInfo & at(ColorAttachmentKey) const noexcept;
    const AttachmentDescriptionInfo & at(DepthStencilAttachmentKey) const noexcept;
    const AttachmentDescriptionInfo & resolveAt(ColorAttachmentKey) const noexcept;  // 前置 hasResolve

    //- canonical 下标：布局知识的唯一出口（§4.2），无 has_ds 参与
    uint32_t slotOf(ColorAttachmentKey) const noexcept;
    uint32_t slotOf(DepthStencilAttachmentKey) const noexcept;
    uint32_t resolveSlotOf(ColorAttachmentKey) const noexcept;      // 前置 hasResolve

    //- discovery：泛型代码用；ds 一路是 optional<Key>，不是 optional<desc>
    std::optional<DepthStencilAttachmentKey> getDepthStencilKey() const noexcept;
    std::optional<ColorAttachmentKey> findColorKey(uint32_t ordinal) const noexcept;  // 越界 = nullopt
    bool hasResolve(ColorAttachmentKey) const noexcept;
    bool hasDepthStencil() const noexcept;
    uint32_t getColorAttachmentCount() const noexcept;      // m_color_descriptions.size()
    uint32_t getResolveAttachmentCount() const noexcept;    // m_resolve_descriptions.size()
    uint32_t getAttachmentCount() const noexcept;           // colors + resolves + has_ds
    AttachmentSetId getSetId() const noexcept;

    //- range view：按角色，不再有「靠布局技巧恰好覆盖」的 multisampleSlots()
    std::span<const AttachmentDescriptionInfo> colorDescriptions() const noexcept;
    std::span<const AttachmentDescriptionInfo> resolveDescriptions() const noexcept;
    const AttachmentDescriptionInfo * getDepthStencilDescription() const noexcept;   // nullptr = 无

    //- 落地：拼 canonical flat 数组（仅静态路径需要）
    std::vector<vk::AttachmentDescription2> flattenDescriptions() const;
private:
    AttachmentSetInfo() noexcept = default;              // 仅 builder 可造
    bool owns(ColorAttachmentKey k) const noexcept { return k.setId() == m_set_id; }
    bool owns(DepthStencilAttachmentKey k) const noexcept { return k.setId() == m_set_id; }
    AttachmentDescriptionInfo & mutableAt(ColorAttachmentKey) noexcept;
    AttachmentDescriptionInfo & mutableAt(DepthStencilAttachmentKey) noexcept;
    AttachmentDescriptionInfo & mutableResolveAt(ColorAttachmentKey) noexcept;
    std::span<AttachmentDescriptionInfo> mutableColorDescriptions() noexcept;
    AttachmentDescriptionInfo * mutableDepthStencilDescription() noexcept;

    AttachmentSetId m_set_id = AttachmentSetId::eInvalid;
    std::vector<AttachmentDescriptionInfo> m_color_descriptions;
    std::vector<AttachmentDescriptionInfo> m_resolve_descriptions;
    std::vector<uint32_t> m_resolve_sources;                   // 与 resolve 同序
    std::vector<std::optional<uint32_t>> m_color_to_resolve;    // 与 color 同序，逆映射
    std::optional<AttachmentDescriptionInfo> m_depth_stencil_description_opt;
};
```

`mutableAt` 是 **typed 重载**，不是 v3 的 `mutableAt(uint32_t canonical_slot)`。
friend 写入方（`RenderTargetInfo2` / 两个 RenderInfo）拿到的是 key，
不该先自己算 canonical 下标再传——那等于把布局知识复制到调用方。

`AttachmentSetInfoBuilder`：唯一的 key 铸造厂。

```cpp
class AttachmentSetInfoBuilder
{
    using Self = AttachmentSetInfoBuilder;
public:
    //- 唯一的 ColorAttachmentKey 来源
    ColorAttachmentKey addColorAttachment() noexcept
    {
        m_color_descriptions.emplace_back();
        return ColorAttachmentKey { static_cast<uint32_t>(m_color_descriptions.size() - 1), m_batch_id };
    }
    //- 唯一的 DepthStencilAttachmentKey 来源 —— 这条使 §4.3 的保证成立
    DepthStencilAttachmentKey enableDepthStencilAttachment() noexcept
    {
        m_has_depth_stencil = true;
        return DepthStencilAttachmentKey { m_batch_id };
    }
    //- resolve 不产生 key：它经源 color 寻址（§4.1.1）
    Self & enableResolve(ColorAttachmentKey source,
        vk::ResolveModeFlagBits mode = vk::ResolveModeFlagBits::eAverage) noexcept;
        // assert(source.setId() == m_batch_id)；记 {source, mode}，重复调用覆盖同一 source

    AttachmentSetInfo build() noexcept;
        // 1. 盖 m_set_id = m_batch_id
        // 2. move colors；逐条 resolve spec：源 color 写 mode、push e1 的 resolve desc、
        //    填 m_resolve_sources 与 m_color_to_resolve 两个方向
        // 3. ds 存在则 emplace optional
        // 4. 重置自身 + 轮换 m_batch_id = next_attachment_set_id()
private:
    AttachmentSetId m_batch_id = next_attachment_set_id();
    std::vector<AttachmentDescriptionInfo> m_color_descriptions;
    std::vector<std::pair<uint32_t, vk::ResolveModeFlagBits>> m_resolve_specs;
    bool m_has_depth_stencil = false;
};
```

对比 v3 的 `build()`：不再需要「ds 必须在 resolve 之前 append」这条**顺序不变量**——
布局在 `slotOf` / `flattenDescriptions()` 里一次定义，builder 只管收集。少一条易错的隐式约定。

删掉 `AttachmentFormatRef` / `AttachmentStateRef`——写入由 `RenderTargetInfo2::setFormat`
与两个 RenderInfo 的 state 写接口经 friend typed `mutableAt` 承担。

## 5. authoring 层：两条路各自 1:1 贴 vk struct

### 5.1 `StaticRenderInfo`：`DynamicStructureChain<vk::RenderPassCreateInfo2>` 封装

与 `SubpassDescriptionInfo` / `AttachmentDescriptionInfo` 同构——**chain 持 root，flat 数组作成员，
`operator const Root &()` 出口**。这样 `pNext` 扩展（`VkRenderPassFragmentDensityMapCreateInfoEXT`、
`VkRenderPassCreationControlEXT` 等）经 `requestExtension<T>()` 原样可达，vkc 不遮蔽。

attachment 表从 set 取（用户不重复填），subpass 直接用已有的 `SubpassDescriptionInfo`。

```cpp
class StaticRenderInfo
{
    using Self = StaticRenderInfo;
    using Root = vk::RenderPassCreateInfo2;
public:
    explicit StaticRenderInfo(AttachmentSetInfo & set) noexcept;
    //- 出口：flat 数组已就位（前置 finalize()，见下）
    operator const Root &() const noexcept { return m_render_pass_info.root(); }

    template <utils::struct_extends_c<Root> T>
    T & requestExtension() noexcept { return m_render_pass_info.template request<T>(); }

    //- ≡ pSubpasses / pDependencies / pCorrelatedViewMasks / flags
    Self & addSubpass(SubpassDescriptionInfo subpass);
    Self & addDependency(SubpassDependencyInfo dependency);
    Self & addCorrelatedViewMask(uint32_t view_mask);
    Self & addFlags(vk::RenderPassCreateFlags flags) noexcept;

    //- attachment 表的 render-scope 半（≡ VkAttachmentDescription2 的 load/store/layout 字段）
    //  静态路径下表由 render pass 拥有，所以写回共享 set 是正确归属，不是 drift
    Self & setLoadStoreOp(ColorAttachmentKey, vk::AttachmentLoadOp, vk::AttachmentStoreOp) noexcept;
    Self & setLoadStoreOp(DepthStencilAttachmentKey, vk::AttachmentLoadOp, vk::AttachmentStoreOp) noexcept;
    Self & setStencilLoadStoreOp(DepthStencilAttachmentKey, vk::AttachmentLoadOp, vk::AttachmentStoreOp) noexcept;
    Self & setInitialFinalLayout(ColorAttachmentKey, vk::ImageLayout initial, vk::ImageLayout final) noexcept;
    Self & setInitialFinalLayout(DepthStencilAttachmentKey, vk::ImageLayout, vk::ImageLayout) noexcept;
    //- resolve 槽经源 color 寻址（§4.1.1）
    Self & setResolveLoadStoreOp(ColorAttachmentKey, vk::AttachmentLoadOp, vk::AttachmentStoreOp) noexcept;
    Self & setResolveInitialFinalLayout(ColorAttachmentKey, vk::ImageLayout, vk::ImageLayout) noexcept;

    //- 便利：按 canonical 布局生成一个全覆盖 subpass（含 depth ref 与 resolve 链接）
    //  只是 addSubpass 的 sugar，用户随时可以自己手写 SubpassDescriptionInfo 全量控制
    SubpassDescriptionInfo makeFullCoverageSubpass() const;

    std::span<const SubpassDescriptionInfo> getSubpasses() const noexcept;
    std::span<const SubpassDependencyInfo> getDependencies() const noexcept;
    const AttachmentSetInfo & getAttachmentSet() const noexcept;
private:
    friend class StaticRender;
    //- 从 set 拉一次 flat attachment 数组，重挂三条数组指针；StaticRender::create 调用
    const Root & finalize() noexcept;

    utils::DynamicStructureChain<Root> m_render_pass_info;
    AttachmentSetInfo * m_set_p;
    std::vector<SubpassDescriptionInfo> m_subpass_infos;      // 拥有各自的 flat ref 数组
    std::vector<SubpassDependencyInfo> m_dependency_infos;
    std::vector<uint32_t> m_correlated_view_masks;
    std::vector<vk::AttachmentDescription2> m_flat_attachments;   // finalize() 时从 set 拉
    std::vector<vk::SubpassDescription2> m_flat_subpasses;
    std::vector<vk::SubpassDependency2> m_flat_dependencies;
};
```

要点：
- **`finalize()` 而非在每个 setter 里重挂**。`SubpassDescriptionInfo` 那类小结构在每次 `add*`
  里 `setXxx(m_flat_...)` 重挂是可行的；但 attachment 表的值**活在 set 里、可被
  `RenderTargetInfo2::setFormat` 后置修改**，authoring 期任何一次快照都可能过期。所以
  attachment 这一条只在 `finalize()`（`create` 前一刻）拉取——**唯一一次快照，不可能读到旧值**。
  subpass/dependency 的 flat 数组顺带在同一处重建，省掉「add 后 vector 扩容导致指针失效」的隐患。
- **不藏 subpass**。用户要 input attachment、preserve、自定义 aspectMask、`pNext` 扩展，
  全部经 `SubpassDescriptionInfo` 原样表达。
- `makeFullCoverageSubpass()` 只是**便利构造**；多 subpass 场景用户自己 `addSubpass` 三次。
- 消除 v3 前身 `RenderingInfo` 的 `AttachmentStateInfo` 平行数组 + `stdv::zip`：
  attachment 表只有一份，在 set 里。

### 5.2 `DynamicRenderInfo`：`DynamicStructureChain<vk::RenderingInfo>` 封装

同样按 chain 封装。`VkRenderingInfo` 的 `pNext` 上挂着一堆真实能力
（`VkRenderingFragmentShadingRateAttachmentInfoKHR`、`VkRenderingFragmentDensityMapAttachmentInfoEXT`、
`VkMultisampledRenderToSingleSampledInfoEXT`、`VkDeviceGroupRenderPassBeginInfo`），
不走 chain 就全部丢失——这正是「不遮蔽 vk 能力」要守的。

```cpp
//- ≡ VkRenderingAttachmentInfo（chain 封装，authoring 半；runtime 半由 begin 填）
//  loadOp/storeOp 默认继承 set；显式 set 则本块覆盖 —— 与 vk struct 逐块自带 loadOp 1:1
class DynamicAttachmentRefInfo
{
    using Self = DynamicAttachmentRefInfo;
    using Root = vk::RenderingAttachmentInfo;
public:
    operator const Root &() const noexcept;              // imageView/clearValue 仍为空，待 begin 填
    template <utils::struct_extends_c<Root> T>
    T & requestExtension() noexcept;                     // 如 VkAttachmentFeedbackLoopInfoEXT

    Self & setLayout(vk::ImageLayout) noexcept;                                    // ≡ imageLayout
    Self & setLoadStoreOp(vk::AttachmentLoadOp, vk::AttachmentStoreOp) noexcept;   // 本块覆盖
    Self & setResolveMode(vk::ResolveModeFlagBits) noexcept;                       // 本块覆盖
    Self & setResolveLayout(vk::ImageLayout) noexcept;
    uint32_t getSlot() const noexcept;                   // canonical 下标（= set.slotOf(...)）
private:
    friend class DynamicRenderInfo;
    utils::DynamicStructureChain<Root> m_attachment_info;
    uint32_t m_slot = 0;
    std::optional<uint32_t> m_resolve_slot_opt;           // 从 set 的配对派生
};

class DynamicRenderInfo
{
    using Self = DynamicRenderInfo;
    using Root = vk::RenderingInfo;
public:
    explicit DynamicRenderInfo(AttachmentSetInfo & set) noexcept;
    template <utils::struct_extends_c<Root> T>
    T & requestExtension() noexcept { return m_rendering_info.template request<T>(); }

    //- ≡ pColorAttachments / pDepthAttachment / pStencilAttachment（有序选择）
    DynamicAttachmentRefInfo & addColor(ColorAttachmentKey,
        vk::ImageLayout = vk::ImageLayout::eColorAttachmentOptimal);
    DynamicAttachmentRefInfo & setDepth(DepthStencilAttachmentKey,
        vk::ImageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal);
    DynamicAttachmentRefInfo & setStencil(DepthStencilAttachmentKey,
        vk::ImageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal);
    Self & setDepthStencil(DepthStencilAttachmentKey, vk::ImageLayout = ...);  // 便利：同槽同 view 两个都设

    //- ≡ viewMask / flags（renderArea 与 layerCount 来自 RenderTarget，属 runtime）
    Self & setViewMask(uint32_t) noexcept;
    Self & addFlags(vk::RenderingFlags) noexcept;

    //- 便利：按 canonical 布局全覆盖（等价于 makeFullCoverageSubpass 的动态版）
    Self & selectAllAttachments();

    std::span<const DynamicAttachmentRefInfo> getColorRefs() const noexcept;
    // depth/stencil ref、viewMask、flags 的 getter…
    const AttachmentSetInfo & getAttachmentSet() const noexcept;
private:
    friend class DynamicRender;
    utils::DynamicStructureChain<Root> m_rendering_info;   // flags/viewMask 在 root；
                                                          // renderArea/layerCount/p*Attachments 属 runtime
    AttachmentSetInfo * m_set_p;
    std::vector<DynamicAttachmentRefInfo> m_color_refs;
    std::optional<DynamicAttachmentRefInfo> m_depth_ref_opt;
    std::optional<DynamicAttachmentRefInfo> m_stencil_ref_opt;
};
```

> **root 的字段一分为二**：`flags` / `viewMask` 是 authoring 值，直接存 root；
> `renderArea` / `layerCount` / 三个 `p*Attachments` 是 **runtime 值**（来自 `RenderTarget` 与每帧
> imageView），由 `DynamicRender::begin` 拷一份 root 出来填。chain 的价值在于 `pNext` 与
> authoring 字段被原样保留并带到 `begin`，而不是让 `DynamicRender` 从零拼一个 `vk::RenderingInfo`。

要点（都是 §2.3 表里的行，不是发明）：
- **depth 与 stencil 分开**——因为 `VkRenderingInfo` 就是两个指针。`setDepthStencil` 是便利重载。
- **没有 initialLayout/finalLayout**——动态路径没这两个字段。进出块的 layout 转换由**上层发 barrier**，
  vkc 不做 barrier 状态机（与 `vkc-pipeline-shader-design.md` §1 的分层一致）。
- **没有 subpass 数组、没有 dependency**——一个 `DynamicRenderInfo` = 一个块。多阶段 = 多个
  `DynamicRender` 对象 + 块间 barrier（§7）。
- **没有 input attachment**——base dynamic 做不到。`local_read` 落地时新增
  `RenderingAttachmentLocationInfo` / `RenderingInputAttachmentIndexInfo` 两个封装，
  作为**能力扩展**加进来，而不是现在就在共享类型里预留一个跑不通的字段。

### 5.3 两条路共享到什么程度（诚实版）

```
共享：AttachmentSetInfo —— format / samples / resolve 配对 / load-store 默认值 / clear 归属槽位
                          + canonical 下标（= framebuffer 槽位 = RenderTarget 槽位）
分岔：编排结构 —— StaticRenderInfo(subpass 图) ⊥ DynamicRenderInfo(单块选择)
```

v3 声称「前半完全共享，只在对象层分岔」——**这是过度承诺**。真实情况：set + image + RenderTarget 共享，
编排从 authoring 期就分岔，因为 Vulkan 在这里本就是两套结构。单阶段场景两边的 authoring 代码各约 3 行，
不值得为「看起来共享」造一个中间类型。

## 6. 对象层：2 类 Render，2 个 scope 类型，2 类 shader 载体

### 6.1 两个 scope：各自 1:1 贴自己那一半 create info

```cpp
//- 静态凭据 ≡ VkGraphicsPipelineCreateInfo 的 { renderPass, subpass }
//  format 由 render pass 隐含，故这里不存 format
class StaticRenderScopeInfo
{
public:
    const vk::RenderPass & getRenderPass() const noexcept { return m_render_pass; }
    const uint32_t & getSubpassIndex() const noexcept { return m_subpass_index; }
    const uint32_t & getColorAttachmentCount() const noexcept { return m_color_attachment_count; }
    const vk::SampleCountFlagBits & getSampleCount() const noexcept { return m_sample_count; }
    const uint32_t & getViewMask() const noexcept { return m_view_mask; }
private:
    friend class StaticRender;                 // 仅 StaticRender::create 可造
    StaticRenderScopeInfo() noexcept = default;
    vk::RenderPass m_render_pass = nullptr;
    uint32_t m_subpass_index = 0;
    uint32_t m_color_attachment_count = 0;     // = 该 subpass 的 color ref 数（不是 set 槽数）
    vk::SampleCountFlagBits m_sample_count = vk::SampleCountFlagBits::e1;
    uint32_t m_view_mask = 0;
};

//- 动态凭据 ≡ VkPipelineRenderingCreateInfo（+ samples，因为该 struct 没有 samples 字段）
class DynamicRenderScopeInfo
{
public:
    operator const vk::PipelineRenderingCreateInfo &() const noexcept { return m_rendering_create_info; }
    std::span<const vk::Format> getColorFormats() const noexcept { return m_color_formats; }
    const vk::Format & getDepthFormat() const noexcept;        // eUndefined = 无
    const vk::Format & getStencilFormat() const noexcept;
    uint32_t getColorAttachmentCount() const noexcept { return m_color_formats.size(); }
    const vk::SampleCountFlagBits & getSampleCount() const noexcept { return m_sample_count; }
    const uint32_t & getViewMask() const noexcept;
private:
    friend class DynamicRender;                // 仅 DynamicRender::create 可造
    DynamicRenderScopeInfo() noexcept = default;
    void rebindPointers() noexcept;            // 拷贝/移动后重挂 pColorAttachmentFormats
    std::vector<vk::Format> m_color_formats;
    vk::PipelineRenderingCreateInfo m_rendering_create_info;   // 指向 m_color_formats
    vk::SampleCountFlagBits m_sample_count = vk::SampleCountFlagBits::e1;
};
```

**为什么 count/samples 两个 scope 都有**：它们不是给 Vulkan 的字段，是给 vkc 用来**自动推导 + 校验
pipeline 状态**的——blend attachment count 必须 = 本块 color 引用数，`rasterizationSamples` 必须
= attachment 的 samples。这两条在两条路上都成立，所以两个 scope 各存一份（值来源不同：静态从
subpass 的 color ref 数 + 表里 samples 算；动态从选择序列 + 表里 samples 算）。

> **`colorAttachmentCount` 恒取本块，不取 set 全集**（硬约束）：
> `pColorAttachmentFormats` / blend attachment count 描述的是**这条 pipeline 实际写出的 color 输出**，
> 必须逐个按序匹配当前块的 color 引用。set 全集含 resolve target、以及可能被别的块用而本块不写的槽——
> 塞全集会同时错 count 与顺序（pipeline `location=N` 对应本块 color 序列的第 N 个，不是 canonical 第 N 个）。
> resolve target 不进 pipeline formats（它在 begin 侧作为 resolve 目标出现）。

### 6.2 `StaticRender` / `DynamicRender`

```cpp
class StaticRender                                        // ≡ VkRenderPass + framebuffer cache
{
public:
    std::error_code create(vk::Device device, const StaticRenderInfo & info) noexcept;
        // 1. info.finalize() → 拉 set.flattenDescriptions() + 重挂 subpass/dependency 数组
        // 2. createRenderPass2Unique(static_cast<const vk::RenderPassCreateInfo2 &>(info))
        // 3. 逐 subpass 造 StaticRenderScopeInfo{ rp, i, colorRefCount(i), samples, viewMask(i) }
    const StaticRenderScopeInfo & scopeAt(uint32_t subpass_index) const noexcept;
    uint32_t getScopeCount() const noexcept;
    void begin(CommandBufferProxy & cmd, const RenderTarget & target) noexcept;  // 查/建 FBO → beginRenderPass
    void nextScope(CommandBufferProxy & cmd) noexcept;                           // vkCmdNextSubpass
    void end(CommandBufferProxy & cmd) noexcept;                                 // vkCmdEndRenderPass
    const vk::RenderPass & getRenderPass() const noexcept;
private:
    vk::Device m_device;
    vk::UniqueRenderPass m_render_pass;
    std::vector<StaticRenderScopeInfo> m_scopes;
    std::unordered_map<std::uint64_t, vk::UniqueFramebuffer> m_framebuffer_cache;   // 1 RP : N target
};

class DynamicRender                                       // ≡ vkCmdBeginRendering，无 GPU 句柄
{
public:
    std::error_code create(vk::Device device, const DynamicRenderInfo & info) noexcept;
        // 1. 从选择序列 + set 预算 attachment 模板（loadOp/storeOp/resolveMode 已定，view/clear 留空）
        // 2. 造 DynamicRenderScopeInfo{ colorFormats[], depth/stencilFormat, viewMask, samples }
    const DynamicRenderScopeInfo & getScope() const noexcept;                    // 单块，无 index
    void begin(CommandBufferProxy & cmd, const RenderTarget & target) noexcept;  // 模板 + runtime → beginRendering
    void end(CommandBufferProxy & cmd) noexcept;                                 // vkCmdEndRendering
private:
    std::vector<vk::RenderingAttachmentInfo> m_color_templates;   // begin 时补 imageView/clearValue
    std::optional<vk::RenderingAttachmentInfo> m_depth_template_opt;
    std::optional<vk::RenderingAttachmentInfo> m_stencil_template_opt;
    std::vector<uint32_t> m_color_slots;                          // 模板 i ← canonical 槽位
    DynamicRenderScopeInfo m_scope;
    vk::RenderingFlags m_flags;
    uint32_t m_view_mask = 0;
};
```

**接口非对称是故意的**：`StaticRender::scopeAt(i)` 对 N 个 subpass，`DynamicRender::getScope()` 只有一个。
因为 Vulkan 在这里非对称——一个 render pass 内含 N 个 subpass，一个 rendering 块就是一个块。
硬造 `DynamicRender::scopeAt(i)` 只是为了「看起来对称」，代价是 vkc 得替用户合成它无从知道的 barrier。

### 6.3 `GraphicsPipeline`：一个类，两个 create 重载

两个重载产物完全相同（`VkPipeline`，`vkCmdBindPipeline`），差别只是同一个 create info 填哪组字段——
所以是**一个类**，不是 v3 的 `SubpassPipeline` + `RenderingPipeline`。

```cpp
class GraphicsPipeline
{
public:
    //- 静态：{renderPass, subpass}；formats 由 RP 隐含，不显式传
    std::error_code create(vk::Device device, const GraphicsPipelineInfo & info,
        const StaticRenderScopeInfo & scope) noexcept;
    //- 动态：renderPass=null + pNext = scope 的 PipelineRenderingCreateInfo（formats 创建期烘死）
    std::error_code create(vk::Device device, const GraphicsPipelineInfo & info,
        const DynamicRenderScopeInfo & scope) noexcept;
    void bind(CommandBufferProxy & cmd) const noexcept;   // vkCmdBindPipeline(eGraphics, ...)
    const vk::PipelineLayout & getPipelineLayout() const noexcept;   // 绑 descriptor / push constant 用
private:
    std::error_code createImpl(vk::Device, const GraphicsPipelineInfo &,
        uint32_t color_count, vk::SampleCountFlagBits, const void * p_next,
        vk::RenderPass, uint32_t subpass) noexcept;       // 两个重载归一到这里
    vk::UniquePipeline m_pipeline;
    vk::UniquePipelineLayout m_pipeline_layout;
};
```

两个重载共同做的（`createImpl`）：
- **blend attachment count 从 scope 补齐**：用户少给的补默认（不透明、写全通道），多给返回
  `errc::color_blend_attachment_count_mismatch`。取代 005 现在手写
  `ColorBlendStateInfo{subpass_info.getColorAttachmentReferenceCount()}`。
- **`rasterizationSamples` 从 scope 补齐/校验**：用户没设则填 `scope.getSampleCount()`，
  设了但不等则报错——这是 MSAA 最常见的静默错配。
- shader module（即弃）+ pipeline layout 内建，与现 `StaticGraphicsPipeline::create` 一致。

**类型安全靠 provenance**：scope 只能从 Render 对象拿到，`StaticRenderScopeInfo` 只能来自 `StaticRender`。
于是「dynamic pipeline 进 render pass」这类非法组合在**重载决议**层面就不存在，不需要跑时 tag。

### 6.4 shader object 不是第三种 pipeline

v3 把它列成 `ShaderObjectPipeline` 是分类错误。看 `VkShaderCreateInfoEXT`：

```c
VkShaderCreateInfoEXT { flags; stage; nextStage; codeType; codeSize; pCode; pName;
    setLayoutCount; pSetLayouts; pushConstantRangeCount; pPushConstantRanges; pSpecializationInfo; }
```

**没有任何 render scope 字段**——没有 renderPass、没有 format、没有 samples。三条推论：

1. **它是 stage 粒度，不是「一条管线」**：`stage` + `nextStage` 描述单个 stage 与其后继；
   一组 stage 用 `VK_SHADER_CREATE_LINK_STAGE_BIT_EXT` 链接成可用组合。
2. **它覆盖 compute（以及 mesh/task）**，不只渲染。把它塞进 `pipeline/graphics/` 命名成
   `...Pipeline` 会遮蔽这一半能力——正是「不遮蔽 vk 能力」这条约束要挡的。
3. **创建期与 render 零耦合**：render scope 只在 **bind/draw 期**以动态状态出现
   （`vkCmdSetRasterizationSamplesEXT` / `vkCmdSetColorBlendEnableEXT` 的 count 等）。

所以它归 **shader 轴**，与 render 轴正交：

```cpp
class ShaderObjectGroup     // shader/ 模块；graphics 或 compute 都用它
{
public:
    std::error_code create(vk::Device device, std::span<const ShaderStageInfo> stages,
        std::span<const vk::DescriptorSetLayout> set_layouts,
        std::span<const vk::PushConstantRange> ranges) noexcept;   // 无 scope 参数
    //- graphics：bind 期才需要 scope（现算 blend count / rasterizationSamples / viewMask 相关状态）
    void bind(CommandBufferProxy & cmd, const DynamicRenderScopeInfo & scope,
        const GraphicsPipelineInfo & state) const noexcept;
    //- compute：无 scope
    void bind(CommandBufferProxy & cmd) const noexcept;
    const vk::PipelineLayout & getPipelineLayout() const noexcept;
private:
    std::vector<vk::UniqueShaderEXT> m_shaders;
    vk::UniquePipelineLayout m_pipeline_layout;    // 绑 descriptor/push constant 仍需它
};
```

`bind` 只接 `DynamicRenderScopeInfo` 是**编译期表达 Vulkan 的硬约束**：shader object 只能在
dynamic rendering 块内绘制（[Vulkan shader_object sample](https://docs.vulkan.org/samples/latest/samples/extensions/shader_object/README.html)）。
没有接受静态 scope 的重载，于是这个错法写不出来。

### 6.5 两轴矩阵（取代 v3 的「恰好 3 类 pipeline」）

|  | `StaticRender`（VkRenderPass） | `DynamicRender`（vkCmdBeginRendering） |
|---|---|---|
| `GraphicsPipeline`（VkPipeline） | ✅ `create(..., StaticRenderScopeInfo)` | ✅ `create(..., DynamicRenderScopeInfo)` |
| `ShaderObjectGroup`（VkShaderEXT[]） | ❌ Vulkan 不允许 | ✅ `bind(cmd, DynamicRenderScopeInfo, state)` |

三个合法格全部由**重载决议**表达，非法格没有对应重载。`ShaderObjectGroup` 另有 compute 用法，
不在这张（render 轴）表里——这正是它不该叫 `...Pipeline` 的原因。

## 7. 多阶段：静态是 subpass 图，动态是多块 + barrier

```
静态：一个 StaticRenderInfo，addSubpass ×N + addDependency ×M
      → 一个 StaticRender，scopeAt(0..N-1)，一次 begin / nextScope ×(N-1) / end

动态：N 个 DynamicRenderInfo → N 个 DynamicRender，各自 getScope()
      → begin/end ×N，块之间由上层发 barrier（cmd.barrier(...)）
```

**vkc 不替用户合成 barrier**：它无法知道跨块的真实依赖（哪些像素、哪个 aspect、要不要 by-region），
猜错就是正确性问题。这与 `vkc-pipeline-shader-design.md` §1 的分层一致——barrier 属于「Vulkan 后端把
RenderGraph 翻译成命令」那一层，vkc 只提供 `cmd.barrier` 原语。

唯一的能力裂缝：**input attachment**。静态路径原生支持（同像素读上一 subpass 输出）；base dynamic
做不到，需 `dynamic_rendering_local_read`（VK 1.4 core）才能在一个块内做到。v4 的处理是
**不在共享类型里预留跑不通的字段**——`local_read` 落地时以 `DynamicRenderInfo` 的扩展方法 +
两个新封装（`RenderingAttachmentLocationInfo` / `RenderingInputAttachmentIndexInfo`）加入，
后端不支持就返回 errc。

## 8. 005 example：两条路的代码

### 8.1 共享前半（两条路逐字相同）

```cpp
auto [width, height] = window.getPixelSize();

//- 形态声明：一个 color 槽（005 无 depth、无 resolve）
vkc::AttachmentSetInfoBuilder builder;
vkc::ColorAttachmentKey color0 = builder.addColorAttachment();
vkc::AttachmentSetInfo attachments = builder.build();          // 布局 = [color0]

//- image-intrinsic：format/samples 的单一权威落在 RenderTarget 侧
vkc::RenderTargetInfo2 render_target_info(attachments);
render_target_info.setFormat(color0, vk::Format::eR8G8B8A8Unorm)
    .setSampleCount(vk::SampleCountFlagBits::e1)               // 写 multisample 前缀，resolve 恒 e1
    .setExtent({width, height});

//- 建 image：format/samples 从 set 取，不第二次声明
for (auto & image : render_target_images) {
    vk::ImageCreateInfo image_info;
    image_info.setImageType(vk::ImageType::e2D)
        .setFormat(attachments.at(color0).getFormat())         // 单一权威
        .setExtent({width, height, 1u}).setMipLevels(1u).setArrayLayers(1u)
        .setSamples(attachments.at(color0).getSampleCount())
        .setUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc);
    // image.create(...) 不变
}
// RenderTarget::build + setColorAttachment 不变
```

### 8.2 静态路径

```cpp
//- StaticRenderInfo ≡ VkRenderPassCreateInfo2：attachment 表来自 set，subpass 直接给
vkc::StaticRenderInfo static_render_info(attachments);
static_render_info.setLoadStoreOp(color0, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore)
    .setInitialFinalLayout(color0, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferSrcOptimal)
    .addSubpass(static_render_info.makeFullCoverageSubpass());
    // 等价手写：addSubpass(vkc::SubpassDescriptionInfo{}.addColorAttachment({0, eColorAttachmentOptimal}))
    // —— 需要 input/preserve/aspectMask/pNext 时就这么写，vkc 不遮蔽

vkc::StaticRender static_render;
if (auto ec = static_render.create(device, static_render_info)) { /* ... */ }

//- pipeline：blend count 与 rasterizationSamples 由 scope 自动补齐
vkc::GraphicsPipelineInfo pipeline_info;
pipeline_info.setShaderProgramInfo(std::move(shader_program_info))
    .setViewportStateInfo(viewport_state_info);                // 不再手填 ColorBlendStateInfo{count}
vkc::GraphicsPipeline pipeline;
if (auto ec = pipeline.create(device, pipeline_info, static_render.scopeAt(0))) { /* ... */ }

//- 循环
static_render.begin(cmd, render_target);      // 查/建 framebuffer → vkCmdBeginRenderPass
pipeline.bind(cmd);
cmd.draw(3, 1, 0, 0);
static_render.end(cmd);
```

### 8.3 动态路径

同一个 `attachments` 与 image；换掉编排那 3 行，pipeline 走另一个重载。

```cpp
//- DynamicRenderInfo ≡ 一个 VkRenderingInfo 块的选择面
vkc::DynamicRenderInfo dynamic_render_info(attachments);
dynamic_render_info.addColor(color0, vk::ImageLayout::eColorAttachmentOptimal)
    .setLoadStoreOp(vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore);
    // 没有 initial/final —— 动态路径没这两个字段（§2.3）

vkc::DynamicRender dynamic_render;
if (auto ec = dynamic_render.create(device, dynamic_render_info)) { /* ... */ }

vkc::GraphicsPipeline pipeline;
if (auto ec = pipeline.create(device, pipeline_info, dynamic_render.getScope())) { /* ... */ }
    // → renderPass=null + pNext = scope 的 PipelineRenderingCreateInfo

//- 循环：进出块的 layout 转换显式发 barrier（vkc 不做 barrier 状态机）
cmd.barrier(/* image: eUndefined → eColorAttachmentOptimal */);
dynamic_render.begin(cmd, render_target);     // → vkCmdBeginRendering
pipeline.bind(cmd);
cmd.draw(3, 1, 0, 0);
dynamic_render.end(cmd);                      // → vkCmdEndRendering
cmd.barrier(/* image: eColorAttachmentOptimal → eTransferSrcOptimal */);
```

**两路差异清单**（诚实版）：①编排 authoring 从一开始就是两个类；②pipeline 走不同重载（同一个类）；
③动态路径**多两条显式 barrier**——这是 Vulkan 的真实代价，v3 把它藏进 `DynamicRender::begin` 是
在替用户猜同步。前半（set / image / RenderTarget / `GraphicsPipelineInfo`）逐字共享。

### 8.4 shader object 路径（可后置）

```cpp
vkc::ShaderObjectGroup shaders;
shaders.create(device, stages, set_layouts, ranges);           // 无 scope
// 循环内：
dynamic_render.begin(cmd, render_target);
shaders.bind(cmd, dynamic_render.getScope(), pipeline_info);   // bindShadersEXT + 全套 vkCmdSet*
cmd.draw(3, 1, 0, 0);
dynamic_render.end(cmd);
```

## 9. 加深度 / 加 resolve = 加数据

```cpp
vkc::ColorAttachmentKey        color0 = builder.addColorAttachment();
vkc::DepthStencilAttachmentKey depth  = builder.enableDepthStencilAttachment();
builder.enableResolve(color0);                                 // 不产生 key（§4.1.1）
vkc::AttachmentSetInfo attachments = builder.build();          // flat = [color0][resolve0][depth]

render_target_info.setFormat(color0, vk::Format::eR8G8B8A8Unorm)
    .setFormat(depth, vk::Format::eD32Sfloat)
    .setSampleCount(vk::SampleCountFlagBits::e4);              // colors+depth 全 e4；resolve 恒 e1
```

- **静态**：`makeFullCoverageSubpass()` 逐 color 问 `hasResolve(k)` 派生 `pResolveAttachments`
  （对位，无 resolve 的位置填 `eAttachmentUnused`）+ depth ref。
- **动态**：`selectAllAttachments()` 给 color0 挂 `resolveMode`/`resolveImageView`（begin 期经
  `resolveSlotOf(color0)` 取 view），depth 与 stencil 各自成 attachment；
  `DynamicRenderScopeInfo` 的 `depthAttachmentFormat` 自动带上。
- 两路的 blend count 都不受 depth/resolve 影响（只数本块 color 引用）。

## 10. 待做项（按依赖顺序）

**Info 层**
1. **Key 类型**（§4.3）：`AttachmentSetId` + `next_attachment_set_id()`、
   `OrdinalAttachmentKey<Role>` / `SingletonAttachmentKey<Role>`（私有构造 + `friend builder` +
   `= delete` 默认构造）、两个 Role 与两个别名。**删掉三个裸 `enum class ...Index`**
   与 `details::attachment_index_c`（后者的三选一约束被 Role 类型取代）。
2. **`AttachmentSetInfo` 重建**（§4.2/§4.4）：三份按角色存储 + 双向 resolve 配对 + `m_set_id`；
   `at`/`resolveAt`/`slotOf`/`resolveSlotOf`/`owns` 校验/discovery/三个 range view/
   `flattenDescriptions()`；typed `mutableAt`。删掉现在的空 public 段与
   `getMultisampleSlots()`（`setSampleCount` 改为分别写 color range 与 ds，§4.2）。
3. **`AttachmentSetInfoBuilder`**（§4.4）：`m_batch_id` 轮换、`enableResolve` 取代
   `addResolveAttachment`（不再返回 key）、`build()` 填双向配对。
4. 删 `AttachmentFormatRef` / `AttachmentStateRef`；`RenderTargetInfo2::setFormat` 从
   `details::attachment_index_c auto` 模板改为**两个 typed 重载**（color / ds），
   `setSampleCount` 按 §4.2 写两处；新增 `setResolveFormat` 不需要——resolve format 跟随源 color，
   由 `flattenDescriptions()` 合成。
5. **`StaticRenderInfo`**（§5.1）：`DynamicStructureChain<vk::RenderPassCreateInfo2>` 封装，
   吃 `SubpassDescriptionInfo`/`SubpassDependencyInfo`，state 写回 set，`finalize()`，
   `makeFullCoverageSubpass()`。**取代 `RenderingInfo`**（连带删掉 `AttachmentStateInfo`
   与 `StaticRendering::create` 里的 `stdv::zip`）。
6. **`DynamicRenderInfo` + `DynamicAttachmentRefInfo`**（§5.2）：分别 chain 封装
   `vk::RenderingInfo` / `vk::RenderingAttachmentInfo`。
7. `RenderTargetInfo`（旧，带 `m_color_formats` 平行数组的那个）退役；`RenderTarget` 的
   attachment 绑定改吃 key（`setColorAttachment(ColorAttachmentKey, image)` /
   `setResolveAttachment(ColorAttachmentKey, image)` / `setDepthStencilAttachment(key, image)`），
   内部经 `set.slotOf` 排 imageView 顺序。

**对象层**
8. **`StaticRenderScopeInfo` / `DynamicRenderScopeInfo`**（§6.1）：私有构造 + friend。
9. **`StaticRender`**（§6.2）：现有 `StaticRender.h` 骨架已在，补
   `create(device, StaticRenderInfo &)`、scope 生成、`scopeAt`、`nextScope`；
   framebuffer cache 沿用 `StaticRendering::begin` 的实现。
10. **`GraphicsPipeline`**（§6.3）：两个 create 重载 + `createImpl`（blend count 补齐、
    samples 补齐/校验）。替换 `StaticGraphicsPipeline`。新增
    `errc::color_blend_attachment_count_mismatch` / `errc::rasterization_samples_mismatch`。
11. **`DynamicRender`**（§6.2）：`CommandBufferProxy` 补 `beginRendering` / `endRendering` 转发。
12. **`ShaderObjectGroup`**（§6.4）：`shader/` 模块，`ShaderObject`（单 stage）之上的链接组；
    取代 `DynamicGraphicPipeline`。可后置到 shader object 需求出现。
13. 重写 005 验证静态路径；新增 006 验证动态路径（§8.3，含两条显式 barrier）。

**推迟**：depth/stencil resolve（走 pNext）；`local_read` + input attachment（§7）；
动态多块的 barrier 辅助；`VK_EXT_dynamic_rendering_unused_attachments`（放宽 format 精确匹配）。

## 11. Trade-offs 汇总

| 议题 | v4 选择 | 理由 |
|---|---|---|
| 两条路的 authoring | **各自一个类，1:1 贴 vk struct** | §2.3 证明编排结构不重合；共享类型会发明 Vulkan 没有的 slot |
| 两个 RenderInfo 的封装形式 | **`DynamicStructureChain<Root>`**，与既有 Info 同构 | `RenderPassCreateInfo2` / `RenderingInfo` 的 `pNext` 上挂着 FSR、density map、MSRTSS 等真实能力，不走 chain 就全丢 |
| attachment 布局 | **按角色分三份存**，flat 数组只在 `flattenDescriptions()` 落地 | 三角色基数不同（N / 0..1 / 配对）；`bool has_ds` 参与下标算术是建模错误 |
| ds 在 flat 数组的位置 | **落尾**，不居中 | 居中只为让 `setSampleCount` 一个 range 写完；分角色存后该 setter 本就该写两处，且 ds 存在性不再污染 resolve 寻址 |
| attachment 句柄命名 | **Key，不是 Index** | ds 的 key 不索引任何东西，只证明「有」；带一个恒 0 的 `uint32_t` 是假信息 |
| key 类型数 | **2 类**（color / ds），删掉 resolve key | Vulkan 两条路都经 color 寻址 resolve；独立 resolve 编号是发明寻址方式 |
| key 的伪造与串用 | **私有构造 + friend builder + `= delete` 默认构造 + set id 盖章** | 「持有 ds key ⟹ set 有 ds」从惯例升级为编译期事实；跨 set 混用运行期校验（编译期方案要求 set 类型随实例变化，代价过大） |
| 共享物 | **只有 `AttachmentSetInfo`** | §2.4 结论 1：能共享的只是每个 attachment 的一组值 |
| render scope | **create 后的只读凭据**，私有构造 | 「持有 scope」⟹「块已建好」；authoring 期它不该存在 |
| scope 是否统一成一个类 | **不统一**，两个类型 | 字段不相交；合并 = variant + 一半死字段。分开则重载决议在编译期定死合法组合 |
| pipeline 类数 | **1 个 `GraphicsPipeline`**，两个 create 重载 | 产物与 bind 命令相同，差别只是同一个 create info 填哪组字段 |
| shader object | **归 shader 轴，`ShaderObjectGroup`** | create info 无 render 字段；stage 粒度；覆盖 compute。叫 `...Pipeline` 会遮蔽能力 |
| dynamic 的 layout 转换 | **不自动发 barrier**，上层显式 | vkc 无从知道真实依赖；barrier 属后端翻译层 |
| dynamic 多阶段 | **多个 `DynamicRender` 对象** | Vulkan 没有 subpass 数组；对称接口的代价是猜同步 |
| blend count / samples | **从 scope 自动补齐 + 校验** | 值已存在于 scope，不是发明；消除最常见的静默错配 |
| input attachment | **不在共享类型预留**，随 `local_read` 落地 | 预留一个 base dynamic 跑不通的字段 = 假承诺 |





