# vkc Hello Triangle 蓝图

> 目标：用 vk_core 画一个三角形，验证 RenderTarget / Rendering / Pipeline / shader_core
> 四条线的接线。示例故意跑一个**模式矩阵**：两个 RenderTarget 轮换 + 三种"渲染方式 ×
> 管线方式"组合轮换，每帧切一种，压测抽象的正交性。
> 本文给蓝图与接线，不复制完整实现；标注哪些是必须先补的 stub。

---

## 0. 先决条件：当前是 stub，必须先补齐

execution 层目前是脚手架。示例能跑之前，按依赖序补这些（`create()` 现在都 `return {}`）：

| 组件 | 现状 | 补什么 |
|---|---|---|
| `RenderTarget`（无 .cpp）| 空 | `create(info)` 分配槽、`build()` mint view；持有 image 引用 |
| `StaticRendering::create` | stub | RenderingInfo⊎RenderTargetInfo 拼 `VkRenderPassCreateInfo2`；`begin` 的 framebuffer cache |
| `StaticGraphicPipeline::create` | stub（bind 会绑 null）| `vkCreateGraphicsPipelines`，从 `GraphicPipelineInfo` 组装 |
| `DynamicRendering::create` | stub；`begin` 未从 target 填 attachment | `begin` 组装 `VkRenderingAttachmentInfo` |
| `DynamicGraphicPipeline`（无 .cpp）| 全空 | shader-object 路径（`VK_EXT_shader_object`）|

**两个 `GraphicPipelineInfo` 的缺口**（补 pipeline 时一并加）：
1. **无 pipeline layout / push-constant 聚合** —— `ShaderProgramInfo` 只带 stage + DS layout。
2. **无 color-attachment format 列表** —— dynamic rendering 建管线要 `VkPipelineRenderingCreateInfo`（formats）。这份 formats 来自 `RenderTargetInfo`，需要一条从 RTInfo 喂给 pipeline 的路。

> `005_hello_static_pipeline` 是过时拷贝（调了已删除的 `device_context.createImage`），**别参考**。
> 模板用 `003_hello_swapchain`。

## 1. Shader：无顶点输入的 triangle.slang

无 vertex buffer：位置由 `SV_VertexID` 索引常量数组生成；颜色可用 draw index（`SV_InstanceID`
或 spec 常量）选。3 顶点、0 binding。

```slang
static const float2 k_positions[3] = { float2(0, -0.5), float2(0.5, 0.5), float2(-0.5, 0.5) };
static const float3 k_colors[3]    = { float3(1,0,0), float3(0,1,0), float3(0,0,1) };

struct VSOut { float4 pos : SV_Position; float3 color : COLOR0; };

[shader("vertex")]
VSOut vsMain(uint vid : SV_VertexID) {
    VSOut o; o.pos = float4(k_positions[vid], 0, 1); o.color = k_colors[vid]; return o;
}
[shader("fragment")]
float4 fsMain(VSOut i) : SV_Target { return float4(i.color, 1); }
```

编译（slang 已支持，返回多 stage 的 `UnitList`）：

```cpp
sc::ShaderCompiler compiler;
auto units = compiler.compileSlangSourceToSpv("shaders/triangle.slang").value(); // UnitList
vkc::ShaderProgramInfo program;
for (const auto & u : units)                       // 每个 Unit = 一个 stage
    program.addStageInfo(vkc::ShaderStageInfo{}
        .setStage(to_vk_stage(u.getStage()))
        .setCode(u.getCode())                      // SPIR-V words,非源码
        .setEntryPoint(u.getEntryPoint()));
```

`VertexInputInfo` 留空（无 binding/attribute），`InputAssembly` 用默认 triangle list。

## 2. Info 三件套（一次构建，示例期不变）

```cpp
// 共享的 RenderTargetInfo:形状 + format（image-intrinsic 侧）
auto rt_info = vkc::RenderTargetInfo{ extent, /*depth*/ vk::Format::eUndefined, samples_e1 }
    .addColorAttachment(color_format /*, resolve eNone*/);

// RenderingInfo:render-scope（load/store/layout/subpass）。单 subpass 常见情况
// 未来可由 RenderingInfo::singleSubpass(rt_info) 工厂派生(见设计文档),现手写:
vkc::RenderingInfo rendering_info;
rendering_info.addAttachmentState(vkc::AttachmentStateInfo{}
        .setLoadStoreOp(vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore)
        .setLayouts(vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferSrcOptimal)) // 之后要 blit-present
    .addSubpass(vkc::SubpassDescriptionInfo{}
        .addColorAttachment(vkc::AttachmentReferenceInfo{0, vk::ImageLayout::eColorAttachmentOptimal}))
    // 关键(见 §6 复用同步):显式 EXTERNAL 依赖,让本趟 render 的 layout transition 等
    // 上一次 blit(present 读同一 color image)完成。默认 EXTERNAL 依赖是 eTopOfPipe/0,
    // 不 wait 任何东西 → 与 blit 读并发 = WAR hazard。
    .addDependency(vkc::SubpassDependencyInfo{}
        .setSubpasses(vk::SubpassExternal, 0u)
        .setStageMasks(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eColorAttachmentOutput)
        .setAccessMasks(vk::AccessFlagBits::eTransferRead, vk::AccessFlagBits::eColorAttachmentWrite));

// GraphicPipelineInfo:program + 各 state
vkc::GraphicPipelineInfo pipeline_info;
pipeline_info.setShaderProgramInfo(std::move(program))
    .setRasterizationStateInfo(vkc::RasterizationStateInfo{}.setCullMode(vk::CullModeFlagBits::eNone))
    .setColorBlendStateInfo(vkc::ColorBlendStateInfo{}.addAttachment())   // 1 个 color,默认不混合
    .setViewportStateInfo(...)  // 或用 dynamic viewport/scissor
    .setDynamicStateInfo(vkc::DynamicStateInfo{}.addDynamicState(vk::DynamicState::eViewport).addDynamicState(vk::DynamicState::eScissor));
```

关键点：**finalLayout = `eTransferSrcOptimal`**，因为渲染完这张 color image 要交给
`swapchain.present()` 做 blit（blit source 要求 transfer-src 布局）。这是烘进 render pass /
dynamic-rendering 声明的（设计文档 §4：layout 端点是声明）。

## 3. RenderTarget × 帧循环:两 RT 轮换 + 三模式轮换

**两个 RenderTarget**：各自持一张 color `vkc::Image`（usage `eColorAttachment | eTransferSrc`），
同一 `rt_info` build 两次（format 签名相同 → 可共享 render pass）。

RT 数量与**安全无关**（安全由 §6 的 barrier 保证，1 张也不会撕裂），只影响**性能**：1 张时
render(N+1) 的 barrier 要等 blit(N) 读完 → stall；2 张轮换时同一张隔两帧才复用，blit 早已完成
→ barrier 不阻塞 → 几乎无等待。这里用 2 张就是为了消 stall + 验证 RT 与策略解耦。

```cpp
std::array<vkc::Image, 2>        colors;         // 各 create(allocator, imageInfo, memAllocInfo)
std::array<vkc::RenderTarget, 2> targets;        // 各 create(rt_info) + build()

// 三种 组合(渲染方式 × 管线方式),每帧切一种
enum class Mode { StaticRender_StaticPipe, StaticRender_DynamicPipe, DynamicRender_DynamicPipe };
```

组合矩阵（注意：static render + dynamic pipeline 是合法的——render pass 提供 renderpass-
compatibility，shader-object 在其中 draw；dynamic render 只能配 dynamic pipeline）：

| Mode | 渲染 | 管线 |
|---|---|---|
| 0 | `StaticRendering` | `StaticGraphicPipeline` |
| 1 | `StaticRendering` | `DynamicGraphicPipeline`（shader object）|
| 2 | `DynamicRendering` | `DynamicGraphicPipeline` |

创建期各建一份：`static_rendering.create(device, rendering_info, rt_info)`、
`dynamic_rendering.create(device, rendering_info)`、`static_pipe.create(device, pipeline_info)`、
`dynamic_pipe.create(device, pipeline_info)`。

### 帧循环（关键：sync 走 timeline，不是 frames-in-flight 索引）

vk_core **没有** FRAMES_IN_FLIGHT 环，用 timeline semaphore + `ResourceLease` 回收。用户要的
"3 帧并行 / 3 command buffer" 落到 vk_core 语义 = **不手动管 3 个 ring slot**，而是每帧从
`QueueContext` 取 proxy、submit 拿 timeline signal、喂给 present；在途资源由 `pinLease` +
timeline 自动保活/回收（并行度由 allocator 的 sub-pool 回收决定，天然 ≥3）。

```cpp
uint64_t frame = 0;
while (running) {
    auto rt_index = frame % 2;
    auto mode     = static_cast<Mode>(frame % 3);
    auto & target = targets[rt_index];
    auto & color  = colors[rt_index];

    auto batch = gfx_queue.allocate(cmd_alloc_info).value();
    auto cmd   = batch.acquireProxy();
    cmd.begin(...);
      cmd.pinLease(color.lease());                 // 保活到本次 submit 完成
      cmd.setViewport(...); cmd.setScissor(...);   // dynamic state
      switch (mode) {
        case Mode::StaticRender_StaticPipe:
            static_rendering.begin(cmd, target); static_pipe.bind(cmd);  break;
        case Mode::StaticRender_DynamicPipe:
            static_rendering.begin(cmd, target); dynamic_pipe.bind(cmd); break;
        case Mode::DynamicRender_DynamicPipe:
            dynamic_rendering.begin(cmd, target); dynamic_pipe.bind(cmd);break;
      }
      cmd.draw(3, 1, 0, 0);                         // 无顶点输入,3 顶点
      (mode == Mode::DynamicRender_DynamicPipe ? dynamic_rendering.end(cmd)
                                               : static_rendering.end(cmd));
    cmd.end();
    batch.collect(std::move(cmd));
    auto render_done = gfx_queue.submit(std::move(batch)).value();   // timeline SemaphoreSubmitInfo

    // blit-present:present 内部 acquire + blit color→swapchain image,wait 在 render_done 上
    swapchain.present(src_offsets, color.handle(), color.lease(), render_done);
    ++frame;
}
```

## 4. 初始化序列（照 003 模板）

`log::init` → 注册 manifest(`register_context/debug_utils/surface/swapchain`) →
`InstanceContext.create` → `RenderDeviceContext.create` → `win::Window.create/show` →
`Swapchain.create(instance, physicalDevice, device, gfxFamily, gfxQueue, wsiHandle)` →
建 2 张 color image（`vkc::Image::create(ctx.getMemoryAllocator(), imageInfo, memAllocInfo)`）→
建 2 个 RenderTarget、两套 rendering、两套 pipeline → 渲染线程跑上面的循环 → 主线程
`window.pollEvents()`；`setResizeCallback([&]{ swapchain.resizeToFit(); })`；退出 `device.waitIdle()`。

## 5. 实现顺序建议

1. `RenderTarget` + `StaticRendering::create`（拼 render pass）+ framebuffer cache → Mode 0 跑通。
2. `StaticGraphicPipeline::create`（`vkCreateGraphicsPipelines`）+ `GraphicPipelineInfo` 补 layout。
3. `DynamicRendering::begin`（从 target 填 attachment）→ 需要 pipeline 侧补 color-format 列表。
4. `DynamicGraphicPipeline`（shader object）→ Mode 1 / 2 跑通。
5. triangle.slang 接 shader_core，喂 `ShaderProgramInfo`。

先做到 **Mode 0 单帧单 RT** 出三角形，再加 RT 轮换、再加 Mode 轮换——每步可独立验证。

## 6. 同步模型（三个方向，全靠同队列 barrier + present 内部节流）

render 与 present 提交到**同一个 `VkQueue`**（gfx queue，003 中 present queue == gfx queue）。
这是下面所有 barrier 方案成立的前提——`vkCmdPipelineBarrier` / subpass `EXTERNAL` 依赖的第一
同步作用域**涵盖同队列上按提交序更早提交的所有命令（跨 submission 也算）**，所以无需 semaphore。

**方向 1 — blit 等 render（同帧）**：`gfx_queue.submit()` 返回 render-complete 的 timeline
`SemaphoreSubmitInfo`，作为 `swapchain.present(..., wait_info)` 传入，blit 在它上 wait。已在循环里。

**方向 2 — render 等上一次 blit（跨帧 WAR）**：复用 `color[i]` 时，本趟 render 的 layout
transition（写 image）必须等上一次 present 对同一 image 的 blit **读**完成，否则 WAR hazard。
- **Static rendering**：靠 §2 里显式声明的 `EXTERNAL` 依赖（`eTransfer/eTransferRead →
  eColorAttachmentOutput/eColorAttachmentWrite`）。**默认 EXTERNAL 依赖是 `eTopOfPipe/0`，不 wait
  任何东西**——必须显式声明，这是最易漏的点。
- **Dynamic rendering（Mode 2）**：没有 render pass、没有 EXTERNAL 依赖，须在 `begin` 前手动
  `vkCmdPipelineBarrier2`，`srcStageMask` 覆盖 blit（`eTransfer/eTransferRead`），把 image 转到
  color-attachment。同一原理，落点从"subpass 依赖"变成"begin 前一条手动 barrier"。

**方向 3 — 节流（CPU 不无限领先）**：`present` 内部每次 `acquireNextImageKHR(timeout=max)`
（`Swapchain.cpp:293`）在无可用 image 时 **CPU 阻塞**；render 与 present 同线程串行，故直接回压
整个循环。上限 ≈ swapchain image count（FIFO 下绑 vsync）。**无需额外 timeline 门控**。

**RT 数量与安全无关**（方向 2 的 barrier 已保证不撕裂）：1 张 → render 等 blit（stall）；2 张
轮换 → 隔两帧才复用，blit 早完成 → 不 stall。纯性能选择。

**若将来改用独立 present queue**：跨队列 barrier 不生效，方向 2 才改为——把 `present()` 返回的
`blit_timeline` signal 按 image index 存起来，复用同一 image 前 `cmd.addWaitInfo(last_blit[i])`。
同队列不需要这套。

