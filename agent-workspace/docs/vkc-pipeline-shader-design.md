# vkc Pipeline / Shader 抽象设计

> 目标：确定 `vk_core/{shader,pipeline}` 两个模块的抽象边界与 Info 结构。
> 核心原则——**vk_core 只固化"几乎不变"的 Vulkan 原语**：封装 + 队列模型 +
> 扩展迭代。RenderGraph 等上层建筑不在本层，反射不在本层，资源去重/缓存不在本层。
> 本文给设计、方案与代码草图，不复制完整实现。

---

## 1. 分层定位：RenderGraph 在上,vk_core 在下

先把边界划清，这决定了下面所有接口的形态。

```
渲染引擎前端    声明 RenderGraph —— 一份后端无关的 description
                  (pass 读写哪些资源、attachment 怎么 load/store、依赖关系)
        ↓ lowering(各后端各自翻译)
Vulkan 后端     把 description 编译成:render scope 选择 + barrier 序列 + 资源生命周期
        ↓ 调用
vk_core         提供原语:Vulkan 封装 + 队列模型 + 扩展迭代
                  (begin/bind/draw/end、submit、cmd.barrier、image/buffer/pipeline)
```

**关键判断**：RenderGraph 是 *description*，不是 *implementation*。它不该知道
`VkRenderPass`、不该知道 barrier、不该知道 timeline semaphore——这些是 Vulkan 后端
"把 graph 翻译成命令"时才出现的概念。LCFEngine 有 `USE_OPENGL` 选项，graph 必须后端
中立。

> 对比：Granite 把 barrier 状态机塞进 RenderGraph，是因为它没有后端无关层、
> graph 直接绑死 Vulkan。Filament 的 FrameGraph 只产出 load/store + usage，
> 同步留给各后端——这才是适合本项目的形态。
>
> 因此 vk_core **不需要**为 graph 定制 `RenderPassInfo→VkRenderPass` 哈希缓存这类
> 接口。vk_core 只提供稳定的 Vulkan 原语；graph 怎么用这些原语是上层后端的事。

vk_core 的 pipeline 模块要提供的，就是下面的 **static/dynamic 两轴模型**——上层后端
翻译 graph 的一个 pass 时，按 profile 选择用哪条路径,把 attachment 信息填进
vk_core 的 Info,然后录制。

---

## 2. 两轴模型(承接 pipeline-rendering-abstraction roadmap)

两条正交轴，4 个类，3 个合法组合：

|            | Rendering(render scope)              | Pipeline(shader 绑定)                  |
| ---        | ---                                    | ---                                      |
| **Static** | `StaticRendering` = VkRenderPass + FBO | `StaticGraphicPipeline` = baked VkPipeline |
| **Dynamic**| `DynamicRendering` = `vkCmdBeginRendering` | `DynamicGraphicPipeline` = shader object |

- `StaticGraphicPipeline` 可配 **任一** rendering(baked PSO 在 render pass 实例和
  dynamic rendering 下都合法)。
- `DynamicGraphicPipeline` 只配 `DynamicRendering`(主动 scope cut，避免 2×2 矩阵；
  排除 `DynamicPipeline + StaticRendering`)。

统一录制接口,static/dynamic 缝隙对上层不可见：

```cpp
rendering.begin(cmd);   // -> beginRenderPass | beginRendering
pipeline.bind(cmd);     // -> bindPipeline    | bindShadersEXT + setViewport...
cmd.draw(...);
rendering.end(cmd);     // -> endRenderPass   | endRendering
```

---

## 3. 贯穿全局的设计准则:"一份 Info,内部自建"

本设计的统一手法(与 `RenderingInfo` 一致)：**调用方填一份声明式 Info,
对象在 `create()` 内部自建所有 Vulkan 资源**。Info 是 single source of truth，
被不同路径以不同方式消费：

- `RenderingInfo`：`StaticRendering` 离线读它建 VkRenderPass/FBO；`DynamicRendering`
  record 时读它拼 `vkCmdBeginRendering`。
- pipeline 输入：`StaticGraphicPipeline` 烘焙期消费全部建 VkPipeline；
  `DynamicGraphicPipeline` 用 SPV+layout 建 shader object,把固定功能留到 record 时
  发 `vkCmdSet*`。

下面分别给两份 Info 与配套接口。

---

## 4. shader 模块:只吃 SPV,反射移出 vk_core

### 4.1 边界:vk_core 不引入 shader_core

vk_core **不依赖 shader_core**,shader 的唯一输入是 **SPIR-V code**。反射
(SPIR-V → descriptor set binding / push constant range)依赖 spirv-cross,那是
shader_core 的职责。一旦 vk_core 只吃 SPV,layout 信息必须作为 **vk-native 数据**从
外面喂进来,而不是 vk_core 自己从 SPV 推。

```
shader_core (spirv-cross)   SPV → 反射 → vk::DescriptorSetLayoutBinding[] + vk::PushConstantRange[]
        ↓  (纯 vk-native 值,不泄漏任何 shader_core 类型)
vk_core/descriptor          bindings[] → VkDescriptorSetLayout
vk_core/shader              { SPV spans + set layouts } → ShaderModule(static) / ShaderObject(dynamic)
vk_core/pipeline            stages + set layouts + GraphicPipelineInfo → VkPipeline / 持有 ShaderObject
```

vk_core 只做"vk-native 描述 → vk 对象",反射这一段彻底在上层。

### 4.2 成本模型:为什么不需要 ShaderProgram

SPV → module → pipeline 是**两段式**成本,绝大部分在后段：

| 步骤 | 成本 | 干了什么 |
|---|---|---|
| `vkCreateShaderModule(spv)` | **很便宜** | 基本只是 memcpy SPIR-V + 轻量校验,**不做 ISA 编译** |
| `vkCreateGraphicsPipelines` | **昂贵** | SPIR-V → GPU 机器码的真正编译在这里 |

推论：一份 SPV 建成多份 module **开销可忽略**。真正贵的是 pipeline bake,
而那由 `VkPipelineCache` 摊销,不靠共享 program 摊销。旁证：`VK_KHR_maintenance5`
允许把 `VkShaderModuleCreateInfo` 直接 chain 进 `VkPipelineShaderStageCreateInfo.pNext`,
**连 VkShaderModule 对象都不建**——module 廉价到可视为瞬态。

> 例外:dynamic 路径的 `vkCreateShadersEXT` 没有后续 pipeline bake,ISA 编译就在
> 创建时,**它是贵的**。但 shader object 与 `DynamicGraphicPipeline` 是 1:1
> (它就是管线本体),不存在"一份 SPV 建多份 object"的冗余,所以不影响决策。

**结论：删除 `ShaderProgram`。** 它原本承载 module + 反射结果这些"重"资产;现在
反射移出、module 廉价即弃、layout 独立,它已无存在价值。"相同 SPV 构造相同 program"
的去重不是 vk_core 的问题。

### 4.3 shader 模块最终构成

```cpp
namespace lcf::vkc {

// 两条路径共同的输入原子:SPV 是 non-owning span,vk_core 用完即弃
struct ShaderStageInfo {
    vk::ShaderStageFlagBits stage = {};
    std::span<const uint32_t> spv;         // 大 blob 由上层 shared_ptr 持有,这里只借
    const char * entry_point = "main";
};

// static 路径用:VkShaderModule 的瞬态 RAII 薄封装,烘进 VkPipeline 后即弃
class ShaderModule {
    std::error_code create(vk::Device device, const ShaderStageInfo & info) noexcept;
    vk::ShaderModule get() const noexcept;
private:
    vk::UniqueShaderModule m_module;
};

// dynamic 路径用:持久产物,DynamicGraphicPipeline 持有,每帧 vkCmdBindShadersEXT
class ShaderObject {
    std::error_code create(vk::Device device,
                           std::span<const ShaderStageInfo> stages,
                           std::span<const vk::DescriptorSetLayout> set_layouts,
                           std::span<const vk::PushConstantRange> ranges) noexcept;
    std::span<const vk::ShaderEXT> getShaders() const noexcept;
private:
    std::vector<vk::UniqueShaderEXT> m_shaders;
};

}
```

要点:
- **`ShaderStageInfo.spv` 是 non-owning span**。vk_core 在 `create()` 内同步消费
  (建 module / 建 shader object)后不再持有 SPV。共享语义(shared_ptr 持有大 blob)
  完全在上层。
- **不去重、不缓存、不反射**。即使同一份 SPV 被反复传入,vk_core 老老实实各建各的。

---

## 5. RenderingInfo:一份描述,喂两条 render scope

`RenderingInfo` 是 render scope 的 single source of truth,字段分两类——
**description**(可离线烘 VkRenderPass / 作 dynamic pipeline 的 format key)与
**binding**(每帧变,喂 framebuffer / begin info)。以单个 attachment 为单位组织：

```cpp
namespace lcf::vkc {

struct RenderingAttachmentInfo {
    // --- description: 烘 VkRenderPass attachment / dynamic pipeline format ---
    vk::Format              format    = vk::Format::eUndefined;
    vk::SampleCountFlagBits samples   = vk::SampleCountFlagBits::e1;
    vk::AttachmentLoadOp    load_op   = vk::AttachmentLoadOp::eClear;
    vk::AttachmentStoreOp   store_op  = vk::AttachmentStoreOp::eStore;
    vk::ImageLayout         layout    = vk::ImageLayout::eColorAttachmentOptimal;
    // --- binding: framebuffer / VkRenderingAttachmentInfo,每帧填 ---
    vk::ImageView           image_view  = {};
    vk::ClearValue          clear_value = {};
    // --- resolve (MSAA) ---
    vk::ResolveModeFlagBits resolve_mode       = vk::ResolveModeFlagBits::eNone;
    vk::ImageView           resolve_image_view = {};
};

class RenderingInfo {
    using Self = RenderingInfo;
public:
    Self & addColorAttachment(const RenderingAttachmentInfo &) noexcept;
    Self & setDepthStencilAttachment(const RenderingAttachmentInfo &) noexcept;
    Self & setRenderArea(vk::Rect2D) noexcept;
    Self & setLayerCount(uint32_t) noexcept;   // 分层渲染
    Self & setViewMask(uint32_t) noexcept;     // multiview
    std::span<const RenderingAttachmentInfo> getColorAttachments() const noexcept;
    const std::optional<RenderingAttachmentInfo> & getDepthStencilAttachment() const noexcept;
    // 便利:供 static-under-dynamic-rendering 的 pipeline 抽 format
    std::vector<vk::Format> getColorFormats() const noexcept;
    vk::Rect2D getRenderArea() const noexcept;
    uint32_t getLayerCount() const noexcept;
    uint32_t getViewMask() const noexcept;
private:
    std::vector<RenderingAttachmentInfo> m_color_attachments;
    std::optional<RenderingAttachmentInfo> m_depth_stencil_attachment_opt;
    vk::Rect2D m_render_area = {};
    uint32_t m_layer_count = 1u;
    uint32_t m_view_mask = 0u;
};

}
```

两个消费者各取所需,同一份 Info 内部自建：
- `StaticRendering::create(device, RenderingInfo)`：用 description 字段建 `VkRenderPass`,
  用 `image_view` 建 framebuffer。`begin(cmd)` 填 clear value + render area。
- `DynamicRendering`：无离线对象,`begin(cmd)` 把每个 attachment 翻译成
  `VkRenderingAttachmentInfo`。

> **设计选择**：`RenderingInfo` 直接持有 `image_view`(每帧重填),以符合 roadmap 的
> `rendering.begin(cmd)` 无参签名——per-frame 的资源选择交给上层渲染层,vk_core 保持
> frame-agnostic。swapchain 就是每张 image 一份 RenderingInfo / framebuffer。
> depth 与 stencil 合成一个 attachment(Vulkan 里通常同一个 image)。

---

## 6. GraphicPipelineInfo:只含固定功能 + 动态状态,不含 shader

shader 来自 `ShaderStageInfo[]`,**不进** `GraphicPipelineInfo`——这样同一份 Info 能
复用到不同 shader,也保证 shader 来源单一。它就是 `VkGraphicsPipelineCreateInfo` 里
那些可变状态块：

```cpp
class GraphicPipelineInfo {
    using Self = GraphicPipelineInfo;
public:
    Self & setVertexInput(std::span<const vk::VertexInputBindingDescription>,
                          std::span<const vk::VertexInputAttributeDescription>) noexcept;
    Self & setInputAssembly(vk::PrimitiveTopology, bool primitive_restart = false) noexcept;
    Self & setRasterization(vk::PolygonMode, vk::CullModeFlags, vk::FrontFace) noexcept;
    Self & setMultisample(vk::SampleCountFlagBits) noexcept;
    Self & setDepthStencil(bool depth_test, bool depth_write, vk::CompareOp) noexcept;
    Self & addColorBlendAttachment(const vk::PipelineColorBlendAttachmentState &) noexcept;
    Self & addDynamicState(vk::DynamicState) noexcept;
    Self & setTessellationPatchPoints(uint32_t) noexcept;
    // getters → vk:: 结构 / span
private:
    // 对应各 state block 的成员
};
```

---

## 7. pipeline layout:两条路径各自内建,靠 compatibility 保证正确

### 7.1 共同货币是 set layouts,不是 pipeline layout

`vkCreateShadersEXT` 的 `VkShaderCreateInfoEXT` **不吃 VkPipelineLayout**,它直接吃
`pSetLayouts`(VkDescriptorSetLayout[])+ `pPushConstantRanges`：

```c
// VkShaderCreateInfoEXT 关键字段
VkShaderStageFlags  nextStage;
size_t codeSize; const void* pCode;                                       // SPV
uint32_t setLayoutCount;        const VkDescriptorSetLayout* pSetLayouts;       // ← set layouts,不是 pipeline layout
uint32_t pushConstantRangeCount; const VkPushConstantRange* pPushConstantRanges;
```

而 static pipeline 烘焙吃 `VkPipelineLayout`。**更关键**：两条路径在 record 时绑
descriptor / push constant 都走 `vkCmdBindDescriptorSets` / `vkCmdPushConstants`,
这俩**都要 VkPipelineLayout**——shader object 也不例外。

整理两条路径的需求：
- **dynamic**：创建 shader object 要 **set layouts + ranges**；record 绑定又要 **pipeline layout**。
- **static**：烘焙和绑定都要 **pipeline layout**。

结论:**共同输入是 set layouts + push constant ranges**(set layouts 是真正必须共享的
身份),pipeline layout 是从前者派生的薄聚合,两条路径各自内建即可。

### 7.2 为什么各自内建是安全的:pipeline layout compatibility

Vulkan 的 **pipeline layout compatibility** 由**定义是否一致**决定,不看 handle：

> 两个 pipeline layout 若由**相同定义的 descriptor set layouts**(set 0..N)+
> **相同的 push constant ranges** 创建,则 "compatible for set N"。

而 descriptor set layout 的"相同"也按**创建参数(flags + binding 数组)**判定,
不看 handle。所以:**两个不同 handle、但 binding 声明完全一致的 pipeline layout,
绑定语义上对 Vulkan 是等价的**——在 layout A 下绑的 descriptor set,切到用 layout B
的管线后依然有效。

代价:它们仍是不同对象,内建产生冗余 layout 对象——但 layout 极廉价,且不影响正确性。
真正按 handle 共享的是 **set layouts**(descriptor set 是针对 set layout 分配的,
这才是必须共享的身份),由 `vk_core/descriptor` 建一次、句柄传入。

### 7.3 统一输入,两条 create

同一份输入(SPV stages + set layouts + ranges + GraphicPipelineInfo)既能建 static
也能建 dynamic,区别只在产物与固定功能的消费时机：

```cpp
// static:吃全部 + rp/formats;固定功能烘进 VkPipeline;内建 module(即弃)+ pipeline layout
class StaticGraphicPipeline {
    // 配 StaticRendering
    std::error_code create(vk::Device device, const GraphicPipelineInfo & info,
        std::span<const ShaderStageInfo> stages,
        std::span<const vk::DescriptorSetLayout> set_layouts,
        std::span<const vk::PushConstantRange> ranges,
        vk::RenderPass rp, uint32_t subpass) noexcept;
    // 配 DynamicRendering(VkPipelineRenderingCreateInfo);color_formats 可从 RenderingInfo.getColorFormats() 取
    std::error_code create(vk::Device device, const GraphicPipelineInfo & info,
        std::span<const ShaderStageInfo> stages,
        std::span<const vk::DescriptorSetLayout> set_layouts,
        std::span<const vk::PushConstantRange> ranges,
        std::span<const vk::Format> color_formats, vk::Format depth_format) noexcept;
    void bind(vk::CommandBuffer cmd) noexcept;          // -> bindPipeline
    vk::PipelineLayout getPipelineLayout() const noexcept;   // 绑 descriptor/push constant 用
private:
    vk::UniquePipelineLayout m_pipeline_layout;          // 内建
    vk::UniquePipeline m_pipeline;
};

// dynamic:吃 SPV + set layouts → ShaderObject;GraphicPipelineInfo 留到 record 时发 vkCmdSet*
class DynamicGraphicPipeline {
    std::error_code create(vk::Device device, const GraphicPipelineInfo & info,
        std::span<const ShaderStageInfo> stages,
        std::span<const vk::DescriptorSetLayout> set_layouts,
        std::span<const vk::PushConstantRange> ranges) noexcept;   // 无 format 耦合
    void bind(vk::CommandBuffer cmd) noexcept;          // -> bindShadersEXT + setViewport/setRasterizer...
    vk::PipelineLayout getPipelineLayout() const noexcept;
private:
    GraphicPipelineInfo m_dynamic_state;                 // record 时翻译成 vkCmdSet*
    ShaderObject m_shader_object;                        // 持久产物
    vk::UniquePipelineLayout m_pipeline_layout;          // 内建,供绑定
};
```

两条路径**输入同形**,差异点：
1. 产物不同:`VkPipeline` vs `VkShaderEXT`(后者持久持有)。
2. `GraphicPipelineInfo` 消费时机不同:static 烘焙期烘死;dynamic record 期翻译成
   `vkCmdSet*`(故 `DynamicGraphicPipeline` 持有 `m_dynamic_state`)。
3. static 多需 rp/formats;dynamic 的 shader object 不和 format 耦合,record 时才定,
   连 format 参数都不需要。

---

## 8. Trade-offs 汇总

| 议题 | 选择 | 理由 |
|---|---|---|
| RenderGraph 归属 | **上层渲染引擎**,vk_core 不实现 | 须后端中立(有 USE_OPENGL);vk_core 只固化稳定原语 |
| 反射归属 | **shader_core**,vk-native 值喂入 vk_core | vk_core 不依赖 spirv-cross;只吃 SPV |
| ShaderProgram | **删除** | module 廉价即弃;反射移出;layout 独立。已无重资产可承载 |
| SPV 共享 | **non-owning span**,大 blob 上层 shared_ptr 持有 | vk_core 用完即弃,无需引用计数 |
| pipeline layout | **两条路径各自内建** | binding 一致即 compatible,descriptor set 跨路径有效;冗余对象廉价 |
| set layouts | **vk_core/descriptor 建一次,句柄共享** | descriptor set 针对 set layout 分配,这是必须共享的身份 |
| 同一份输入建两种 pipeline | **是**,输入同形 | 差异只在产物与固定功能消费时机 |
| shader 进 GraphicPipelineInfo | **否** | 同 Info 复用不同 shader;shader 来源单一 |

---

## 9. Landing 顺序

1. **shader 模块**：`ShaderStageInfo` / `ShaderModule`(原 `Shader.h` 更名)/
   `ShaderObject`;删 `ShaderProgram`。
2. **info_structs**：落 `RenderingInfo` / `RenderingAttachmentInfo` /
   `GraphicPipelineInfo`;退役 `Pipeline.h` stub。
3. **descriptor**：`vk_core/descriptor` 补 set layout 创建(vk-native binding → VkDescriptorSetLayout)。
4. **StaticRendering + StaticGraphicPipeline**：fullscreen-triangle 例子打通到 swapchain
   (slang 字面量顶点,无 buffer/image/descriptor)。
5. **DynamicRendering + DynamicGraphicPipeline**：确认 device-level dispatch 解析
   `vkCmd*EXT`;两个例子(static+dynamic、dynamic+dynamic)输出一致。
6. **锁定** `begin(cmd)`/`bind(cmd)`;类型层面强制 `DynamicPipeline → DynamicRendering`。



