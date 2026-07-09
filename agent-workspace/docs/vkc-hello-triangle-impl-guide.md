# 005 hello_static_pipeline 实现指南

> 蓝图已搭好、编译通过。运行时 validation log（`build/msvc/bin/Debug/logs/debug.log`）暴露 4 处
> 未实现，对应 4 个 VUID。本文按"日志症状 → 根因 → 补哪个文件"逐条给实现，不复制无关代码。

四条互相独立，可分别验证。建议顺序：3 → 4 →（1、2 可并列）。

---

## 症状 1：`VUID-VkShaderModuleCreateInfo-pCode-08740`（DrawParameters capability）

**日志**：`SPIR-V Capability DrawParameters was declared, but ... shaderDrawParameters OR VK_KHR_shader_draw_parameters` required。

**根因**：`triangle.slang` 用 `SV_VertexID`。Slang 编 SPIR-V 时把它降级为 `gl_VertexIndex`，
并声明 `DrawParameters` capability（vertexIndex 含 baseVertex 语义）。该 capability 需要设备特性
`shaderDrawParameters` 或扩展 `VK_KHR_shader_draw_parameters` 显式开启——例子没开。

**补哪里**：设备特性开启，在 `RenderDeviceContext` 创建前的 manifest 阶段（`entry::register_*`
或直接给 `DeviceExtensionManifest`）加：
- 特性法（推荐，1.1+ core）：请求 `vk::PhysicalDeviceVulkan11Features::shaderDrawParameters = true`。
  用你们的 feature manifest 宏 `LCF_VKC_UTILS_FEATURE_BIT(&vk::PhysicalDeviceVulkan11Features::shaderDrawParameters)`。
- 或扩展法：manifest 加 `VK_KHR_shader_draw_parameters`。

（备选：若不想开特性，可改 shader 避免 baseVertex 语义——但 `SV_VertexID` 几乎必然触发，
开特性是正解。）

## 症状 2：`viewportCount/scissorCount == 0`（VUID-04135 / 04136）

**日志**：`pViewportState->viewportCount can't be 0 unless VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT`；
scissor 同理。

**根因**：例子构建 `GraphicsPipelineInfo` 时**完全没设 viewport state，也没设 dynamic state**
（`main.cpp:187-189` 只 setShaderProgramInfo）。于是 `ViewportStateInfo` 空 →
viewportCount=0、scissorCount=0，而又没声明 with-count 的 dynamic state → 违规。

**补哪里**：`main.cpp` 构建 pipeline info 时二选一：
- **静态视口**（最简，先出图）：`graphic_pipeline_info.setViewportStateInfo(vkc::ViewportStateInfo{}
  .addViewport(0,0,width,height).addScissor(0,0,width,height))`。
- **动态视口**（推荐，随 resize 不用重建管线）：
  `setDynamicStateInfo(vkc::DynamicStateInfo{}.addDynamicState(eViewport).addDynamicState(eScissor))`，
  且 `ViewportStateInfo` 仍需 `viewportCount == scissorCount == 1`（值忽略，count 要对）——
  用 `eViewport`（非 with-count）时 count 必须 >0；录制时 `cmd.setViewport/setScissor`。
  若想 count 也动态，改用 `eViewportWithCount`/`eScissorWithCount`，那时 ViewportStateInfo 可空。

先用静态视口跑通，再按需换动态。

## 症状 3：`framebuffer is VK_NULL_HANDLE`（begin render pass）+ RenderTarget 空实现

**日志**：`vkCmdBeginRenderPass(): pRenderPassBegin->framebuffer is VK_NULL_HANDLE`。

**根因**：两处连锁未实现——
1. `RenderTarget::create`（`RenderTarget.cpp`）直接 `return {}`，**不建 image/attachment**。
   `m_attachments` 空，`getAttachment(0)` 越界，且没 view 可组 framebuffer。
2. `StaticRendering::begin`（`StaticRendering.cpp`）里 `vk::Framebuffer framebuffer;` 是未初始化
   局部量，注释 `// get from cache`，framebuffer cache 从没填 → 传了 null handle。

**补哪里**（这是最大一块，按蓝图 §2/§5）：

**RenderTarget — 接口改为 `build` + `set`（替换 `create`）**：语义拆开——`build` 定形状、
`set` 填内容，image 由外部提供（蓝图"RenderTarget 是模板"）。

- **`build(const RenderTargetInfo & info)`**：读 schema，按 color format 数（+depth）**分配槽**，
  每槽记下 `AttachmentDescription`（aspect 从 format 推），**不碰 image、view 空**。顺带
  `m_render_area = {{0,0}, info.getExtent()}`、按槽数填默认 `m_clear_values`。
- **`setColorImage(uint32_t color_index, const Image & image)`**：填第 index 个 color 槽，
  **当场用该槽的 desc mint view**（eager，无需单独 finalize——view 要 image，image 到 set 才有）。
  越界/类型不符 → error。
- **`setDepthStencilImage(const Image & image)`**：填 depth 槽；**schema 无 depth 槽 → 返回 error，
  不静默**。
- `setColorResolveImage(color_index, image)`：samples>1 的 resolve 目标；hello-triangle 用不到，
  接口先留。
- **消费侧**：`getAttachmentViews()` 按 canonical 顺序 `[color..][depth]` 返回 view span，
  供 `StaticRendering::begin` 组 framebuffer。

内部：`struct Slot { AttachmentDescription desc; Attachment attachment; }`；`vector<Slot> m_color_slots`
+ `optional<Slot> m_depth_slot`。build 定 desc，set 填 attachment。

`main.cpp` 对应改：外部建好的 `render_target_images` → `build(info)` 后 `setColorImage(0, image)`
填入——**消除现在"image 外部建一遍、`create(info)` 再忽略"的重复**。

**StaticRendering — framebuffer cache**：`begin` 里用 target 的 attachment views 查/建 framebuffer：
- cache key = 这组 view 句柄 + extent + layers（见设计文档 RenderTarget §）。
- miss 时 `vkCreateFramebuffer`（renderPass=`m_render_pass`，pAttachments=target 的 views，
  width/height/layers 取 target），存入 `m_framebuffer_cache`，附 `ResourceLease` 钉住 image 存活。
- 用查到的 framebuffer 填 `render_pass_info.setFramebuffer(...)`。

**StaticRendering::create — attachment descriptions**：现在 `render_pass_info.setAttachments()` 被注释掉
（`StaticRendering.cpp:24`），render pass 没有 attachment。按蓝图 §3 拼接：
`vk::AttachmentDescription2[i] = AttachmentStateInfo[i](RenderingInfo) ⊎ (format,samples)[i](RenderTargetInfo)`。
没有 attachment description，症状 4 也无法解决。

## 症状 4：`ShaderOutputNotConsumed` + subpass 无 color attachment

**日志**：`fragment shader writes to output Location 0 but there is no VkSubpassDescription::pColorAttachments[0]`。

**根因**：`main.cpp:177-180` 建的 `SubpassDescriptionInfo` 只 `setBindPoint`，**没 addColorAttachment**；
且 `RenderingInfo` 没 addAttachmentState。于是 render pass 0 个 attachment、subpass 0 个 color ref，
而 fragment shader 写 location 0 → 无处可去。

**补哪里**：`main.cpp` 构建 rendering_info 时（对齐蓝图 §2）：
- `RenderingInfo.addAttachmentState(AttachmentStateInfo{}.setLoadStoreOp(eClear, eStore)
  .setLayouts(eUndefined, eTransferSrcOptimal))` —— finalLayout 用 `eTransferSrcOptimal`
  （之后 blit-present 要 transfer-src）。
- `SubpassDescriptionInfo.addColorAttachment(AttachmentReferenceInfo{0, eColorAttachmentOptimal})`。
- 与症状 3 的 `StaticRendering::create` attachment 拼接配合：AttachmentStateInfo 提供 load/store/layout，
  RenderTargetInfo 提供 format/samples，按 index 合并成 `AttachmentDescription2`。

## 依赖关系与验证顺序

- **3 和 4 是同一条链**（render pass attachment + framebuffer），必须一起才有画面：
  先补 `RenderingInfo` 的 attachmentState + subpass color ref（症状 4），再补
  `StaticRendering::create` 的 attachment 拼接 + `RenderTarget` 建 image/view + framebuffer cache（症状 3）。
- **1、2 独立**，可先补（开特性、给 viewport state），先让 `vkCreateShaderModule` /
  `vkCreateGraphicsPipelines` 不报错。
- 建议：先 1+2（管线能创建）→ 再 4（render pass 有 attachment）→ 再 3（RenderTarget+framebuffer）
  → 出三角形。每步 log 应少一类 VUID。

## 一个附带清理

`main.cpp` 里 image 在外部建、又调 `render_target.create(info)`——重复且 create 目前忽略这些 image。
按症状 3 的 `build` + `set` 定案统一：外部建 image → `build(info)` 定形状 → `setColorImage(i, image)`
填入并 mint view。删掉旧 `create(info)` 路径，别让 image 建两遍。
