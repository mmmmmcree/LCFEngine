# vkc AttachmentSetInfo 单一权威设计

> 目标：消除 attachment 描述信息在 `RenderTargetInfo` 与 `RenderingInfo` 间的**平行数组**，
> 收敛为**一处权威 + 投影**。权威持有 attachment 的全部叶子数据，其余 Info 从它投影得到；
> 用户操纵投影即写回权威，结构上杜绝 drift。
> 本文是对 [`vkc-rendertarget-rendering-design.md`](./vkc-rendertarget-rendering-design.md) §3
> 的演进：把「两个 authoring 源按 index 拼接」翻转为「单一权威向下投影子集」。
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

职责分层本身没错（format 是资源层、load/store 是 pass 语义层，见前文 §3）。错的是
**没有任何一方是 attachment 集合的权威**，count 与顺序靠人工同步。

## 2. 权威不是「rendering」，而是「attachment 集合」

pipeline 只依赖一个 subpass，subpass 和 render target 都只取 attachment 的子集。但让
`RenderingInfo`（render pass）当权威有两个坑：

1. **混了两种权威**：attachment 叶子数据（format/samples/ops/layout）与 subpass 拓扑图
   （谁引用哪个 index）。render target 只要前者，不该绑上 subpass 图。
2. **dynamic rendering 没有 render pass / subpass**。`VkAttachmentDescription` 只存在于
   render pass 模型。把权威绑在 rendering 上，dynamic 路径又得把耦合请回来。

结论：权威是 **attachment 集合本身**（纯叶子数据），是 rendering 与 render target 的**共同上游**。
三个消费者各取不同子集，共同压在 `format + samples` 这条脊上：

| 消费者 | 取的子集 | 丢弃 |
|---|---|---|
| RenderTargetInfo | format, samples（+ 自己的 extent / 绑的 image） | load/store, layout, 角色 |
| Subpass reference | index + layout + 角色(color/depth) | format, samples, load/store |
| AttachmentDescription | format, samples, load/store, initial/final layout | index, 角色 |

## 3. `AttachmentSetInfo`：build 冻结形态，投影写回

模型：**建 shape → `build()`（冻结形态 + 填合理默认 + reserve）→ 操纵投影 = 写回权威**。

build 后内部分两层，这个区分决定投影用「引用」还是「快照」：

- **形态层（冻结）**：attachment 数量、每个 color/depth、format、samples。build 后不可变。
- **可变层**：load/store、layout 等 pass 语义。build 只填默认，之后 `set(index,...)` 可改。

### 3.1 投影分两档（按生命周期，不能一刀切成「全是视图」）

判据是**投影对象活多久、何时被读**：

| 投影 | 生命周期 | 读取时机 | 形态 |
|---|---|---|---|
| `AttachmentStateRef`（单个 attachment 的 load/store/layout） | 瞬时（setup） | — | **写回视图** `{set&, index}` |
| `RenderingInfo` | 瞬时（只在 `StaticRendering::create` 用一次） | create 时 | **引用 set**（读可变层） |
| `RenderTargetInfo` | **长命**（render loop 每帧 begin 都用） | 每帧 | **快照冻结层**，不持引用 |

关键在最后一行：`RenderTargetInfo` 比 set 活得久（set 是 setup 期临时 builder，RT 撑到渲染循环）。
它只需 format/samples 这些**冻结层**数据，冻结即不会 drift，**快照恰好安全又解耦**；若持 set 引用，
则 set 得陪整个 render loop 存活，生命周期倒挂。

所以「投影 = 写回 set」对**瞬时消费者**（state、rendering）成立；对**长命消费者**（render target）
应快照冻结子集。这是「everything is a view」表述里唯一的补丁，依据明确：**读得晚 + 活得久 → 必须快照**。

### 3.2 三条不变量（写回引用的代价，用 build/freeze 摁住）

1. **freeze 后禁 `add`**：add 会让内部 vector 重分配，已发出的 `AttachmentStateRef` 与
   `RenderingInfo` 引用全部悬空。`build()` 后 add 直接返回 error；freeze 时 `reserve` 到位杜绝 realloc。
2. **set 必须比其视图/引用者活得久**：`RenderingInfo` 引用 set，故 set 要活过
   `StaticRendering::create`。惯例是 set 声明在最前，天然满足。
3. **持视图期间禁拷贝 set**：副本的视图仍指向原件。禁拷贝，或约定「拷贝后重取视图」。
   `RenderTargetInfo` 因是快照，**不受这三条约束**——再次佐证它该快照。

## 4. API 形态

`AttachmentSetInfo`：

```cpp
// 构建（build 前，权威数据一次填全）
uint32_t addColorAttachment(vk::Format, vk::ResolveModeFlagBits resolve = eNone);  // 返回 index
uint32_t setDepthStencilAttachment(vk::Format);                                    // 返回 index
Self &   setSampleCount(vk::SampleCountFlagBits);                                  // 作用于全体(resolve 除外)
Self &   build();                                                                  // 冻结形态 + 默认 + reserve

// 操纵可变层（build 后，写回视图）
AttachmentStateRef stateAt(uint32_t index);   // {AttachmentSetInfo&, index}，setter 直接写 set

// 投影（factory）
RenderTargetInfo    projectRenderTarget(vk::Extent2D extent) const;                // 独立快照(format/samples)
vk::ImageCreateInfo projectImageCreateInfo(uint32_t index, vk::Extent3D) const;    // 填 format/samples/type，usage 留给调用方
RenderingInfo       projectSingleSubpassRendering() const;                         // 引用 set + 生成单 subpass 拓扑
SubpassDescriptionInfo projectDefaultSubpass() const;                              // 全 color + depth，标准 layout

// 查询
uint32_t getColorAttachmentCount() const;
bool     hasDepthStencil() const;
vk::SampleCountFlagBits getSampleCount() const;
```

配套改动：
- `AttachmentStateInfo` 补 `format` + `samples`，成为 `VkAttachmentDescription` 的完整镜像 / 权威元素。
- `RenderingInfo` 内部持 `AttachmentSetInfo`（引用或内嵌），而非裸 `vector<AttachmentStateInfo>`；
  自己只加 subpass 拓扑 + dependency。
- `StaticRendering::create` **去掉 `RenderTargetInfo` 形参**——format/samples/depth 现都在
  rendering_info 自带的 set 里，不再 zip，硬编码 samples 的 bug 一并消失。
- color/depth 角色可由 format 推断，不必显式存。

## 5. 005 example 写法

```cpp
auto [width, height] = window.getPixelSize();

//- 权威：建 shape → build（冻结形态 + 填默认值）
vkc::AttachmentSetInfo attachments;
uint32_t color0 = attachments.addColorAttachment(vk::Format::eR8G8B8A8Unorm);
attachments.build();

//- 操纵投影 = 写回 set（瞬时写回视图；编辑顺序无关）
attachments.stateAt(color0)
    .setLoadStoreOp(vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore)
    .setLayouts(vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferSrcOptimal);

//- 长命消费者：快照冻结层（format/samples），自带 extent，不持 set 引用
vkc::RenderTargetInfo render_target_info = attachments.projectRenderTarget({width, height});

for (auto & image : render_target_images) {
    vk::ImageCreateInfo image_info = attachments.projectImageCreateInfo(color0, {width, height, 1u});
    image_info.setUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc);
    // image.create(...) 不变
}
// render_target.build / setColorAttachment 不变

//- 瞬时消费者：引用 set，create 时读回可变层
vkc::RenderingInfo rendering_info = attachments.projectSingleSubpassRendering();
vkc::StaticRendering static_rendering;
static_rendering.create(device, rendering_info);   // 读 set 的 attachment 描述，无 zip

vkc::ColorBlendStateInfo color_blend_state_info{attachments.getColorAttachmentCount()};
```

`eR8G8B8A8Unorm` 只出现一次；subpass、render pass attachment desc、blend count 全从
`attachments` 派生，无第二份数据需手工对齐。`stateAt(...).set...()` 可放在
`projectSingleSubpassRendering()` **之后**——rendering 引用 set、读得晚，写回随时可见；
`render_target_info` 快照冻结层，放哪都安全。这正是写回视图相对快照的收益：**编辑顺序无关，永不 drift**。

## 6. 加深度 = 加数据，不是改流程

```cpp
uint32_t depth = attachments.setDepthStencilAttachment(vk::Format::eD32Sfloat);
attachments.build();
attachments.stateAt(depth).setLoadStoreOp(vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eDontCare);
// projectRenderTarget 自动带上 depth 槽位
// projectSingleSubpassRendering 自动生成 depth attachment desc + depthStencil reference
// blend count 不受影响（depth 非 color）
```

对比当前模型：加深度要同时动 `create` 的 zip 实现、subpass、attachment state 三处并保证对齐。

## 7. dynamic rendering 顺带解锁

`AttachmentSetInfo` 与「render pass vs dynamic」正交：
- **render pass 路径**：喂 `RenderingInfo` 的 attachment 描述 + subpass 引用。
- **dynamic 路径**：subpass 选出的 color 子集 → `PipelineRenderingCreateInfo.colorAttachmentFormats`，
  depth format 同理；load/store 移到 begin 时的 `VkRenderingAttachmentInfo`（与 image view 同居，
  平行数组问题从根消失）。

同一 `attachments` 喂两条路，`StaticGraphicsPipeline` 加 dynamic 重载不用新造 info：

```cpp
std::error_code create(vk::Device, const GraphicsPipelineInfo &, const RenderTargetInfo &) noexcept;
```

内部挂 `vk::PipelineRenderingCreateInfo` 到 pNext，`setRenderPass(nullptr)`。

## 8. 待做项（按依赖顺序）

1. **`AttachmentStateInfo` 升级**：补 `format` + `samples` 字段（成为权威元素）。不动现有调用。
2. **`AttachmentSetInfo` 落地**：`add* / build / stateAt` + 四个 project 方法；投影分档
   （state/rendering 引用，render target 快照）；三条不变量（freeze 禁 add + reserve、
   set 最长命、持视图禁拷贝）。
3. **`RenderingInfo` 改造**：内部持 `AttachmentSetInfo`，只加 subpass 拓扑 + dependency。
4. **`StaticRendering::create` 去 `RenderTargetInfo` 形参**：attachment desc 从 rendering_info
   自带 set 生成；samples 从 set 取；depth 接入。zip 与硬编码 samples bug 消失。
5. **重写 005 example** 验证（§5）。
6. **（可选）`StaticGraphicsPipeline` dynamic 重载**（§7）+ `DynamicRendering::begin/end`。

**依赖前置**：本设计假定 [`vkc-rendertarget-rendering-design.md`](./vkc-rendertarget-rendering-design.md)
§2 的 `RenderTarget` 模板+build 模型已落地或并行推进；resolve 单一权威仍归 `RenderTargetInfo`
（本文投影方法从 set 生成 RTInfo 时需一并带出 per-color resolve 声明）。

**推迟**：depth/stencil resolve；多 subpass 场景的 `projectSubpass(...)` 定制路径
（单 subpass 先行，多 subpass 用手写 `SubpassDescriptionInfo` + `colorReference(i, layout)`）。
