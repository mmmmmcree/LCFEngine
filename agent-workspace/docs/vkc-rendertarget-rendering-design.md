# vkc RenderTarget / Rendering 抽象设计

> 目标：确定 `vk_core/pipeline/graphics` 下渲染目标与渲染策略的抽象边界，以及
> Image/Attachment 资源层、Info 结构体的组织方式。
> 核心原则——**信息按"天然拥有者"单一归属，vk 结构在 build 时按 index 拼接而成；
> layout 转换是声明式的，跨 pass 追踪留给上层 RenderGraph。**
> 本文给设计、方案与待做项，不复制完整实现。

---

## 1. 资源层：Image → Attachment

三层职责，各回答一个问题：

| 层 | 拥有 | 回答 |
|---|---|---|
| `Image` | memory(`ResourcePtr<Memory>`)+ `ImageDescription` + `vk::Device` | "这块 GPU 资源是什么" |
| `Attachment` | `Image` 引用 + `vk::UniqueImageView` + subresource 选择 | "拿它的哪一片、怎么当渲染目标" |
| `RenderTarget` | 一组 `Attachment` + 组属性(samples/extent/layers) | "这趟 pass 往哪些地方写" |

关键决定：

- **`Image` 自己生成 view**（`createView(range, viewType)`），因为真正的资源是 `Memory`，
  `Image` 已是可共享的资源句柄，持有 device 即可 mint view。**不缓存**——view 由调用方
  (Attachment) 拥有。共享 `Image`、各消费者自建 view 是有意的模式：绕开"共享 image +
  内部 view 缓存"的生命周期地狱。cache 只在将来出现"同 subresource 反复 mint"时才加。
- **`ImageDescription` 是 `vk::ImageCreateInfo` 的严格子集**（format/extent/type/mips/
  layers/samples/usage），投影方向 `ImageCreateInfo → Description`（可推导），不含 aspect
  （aspect 是 view 侧概念）。存进 `Image` 只读。
- **layout 不进 `Image` 也不进 `Attachment`**。layout 是 per-subresource 的共享事实，
  放进可拷贝的 `Image` 成员会分裂（同一 VkImage 两份状态）；放进 `Attachment` 在
  attachment 范围重叠时也会漂移。结论见 §4。

## 2. RenderTarget 模型：模板 + 填 Image + build

`RenderTarget` 是**模板**：形状固定、Image 外部填入。用法：

```
1. 声明 schema(RenderingInfo + RenderTargetInfo，见 §3)
2. RenderTarget 按 info 分配好槽(color 数 / 有无 depth / 哪些 color 带 resolve)
3. 外部构造 Image，按 index 填槽;DS 单独槽,setDSImage(无 DS 声明则返回 error,不静默)
4. build(): 校验(samples 统一 / resolve==1x / extent 一致 / aspect 匹配 / 范围 disjoint)
           + 生成 Attachment(mint view)
5. Rendering::create(renderingInfo, renderTargetInfo) 建 render pass(从 info 取 format)
6. Rendering::begin(cmd, rt) 用 rt 的 view 查/建 framebuffer
```

- **RenderTarget 不拥有 Image**（外部填、refcount 引用）。swapchain 走 blit-present，
  RT 恒离屏，不需把 swapchain image 包成 RT。
- **per-frame**：模板生成一次，为每个 frame-in-flight 槽各填各自 Image、各 build 一次，
  得 N 个 format 签名相同的具体 RT，共享同一 render pass，view 各自稳定。
- **RT 对渲染策略无知**：同一 RT 喂给 Static 或 Dynamic 皆可。

## 3. 信息拆分与拼接：RenderingInfo ⊎ RenderTargetInfo

一个 attachment 由两类事实描述，各归唯一 authoring 源（推广 `AttachmentStateInfo` 的 seam）：

- **image-intrinsic** → `RenderTargetInfo`：format、samples、extent、per-color resolve 声明。
- **render-scope** → `RenderingInfo`：load/store op、layout transition、flags、subpass、dependency。

`vk::AttachmentDescription2[i] = AttachmentStateInfo[i](RenderingInfo) ⊎ (format,samples)[i](RT)`，
在 `Rendering::create` 按 index 拼。**这个拼接只发生在 attachment 数组这一处**；subpass/
dependency 归 RenderingInfo 独有，format/extent 归 RT 独有，不需要拆。

**canonical index 布局**（两边必须遵守的契约）：
```
authored 段:  [color 0..N-1][depth?]      ← AttachmentStateInfo 与 RT 同序同 index
合成段:       [resolve targets...]         ← 拼接方按 resolve 声明合成、追加
```

拼接时的联合不变量（非任一方 own，必须断言）：
`attachmentStates.size() == colorFormats.size() + hasDepth`；subpass color ref 数 == N。
index 寻址贴合 Vulkan（render pass attachment 本就是数组下标），代价是重排两边会静默错位，
靠契约文档 + count 断言兜底。

**resolve 单一权威在 RenderTargetInfo**（per-color `ResolveModeList`，`eNone`==不 resolve）：
- 拼接方据 `isColorResolved(i)` 合成 resolve attachment（format 同 color、samples=1）、
  填 subpass `pResolveAttachments`。**收掉 `SubpassDescriptionInfo::addResolveAttachment`
  的手写路径**（否则两处 author，冲突）。
- **MSAA/resolve 的 store/layout 分发**：authored 的最终态（storeOp=Store + finalLayout）
  贴给**存活者**——resolve 时即 resolve target；MSAA source 强制 `storeOp=eDontCare`
  （transient）。后者也解锁 tiler 的 `LAZILY_ALLOCATED`（多采样数据不落主存）。
- depth/stencil resolve（`VK_KHR_depth_stencil_resolve`）未建模，`RenderTargetInfo` 留 TODO。

## 4. Layout 转换：声明式，跨 pass 追踪归 RenderGraph

render pass 的 `initialLayout`/`finalLayout` **烘死在 `VkRenderPass` 创建时**（`AttachmentDescription2`），
不是运行期设置：
- **finalLayout**：驱动保证 pass 结束时转到它 —— post 状态静态已知。
- **initialLayout**：pass **假定** image 已处于它（或 `UNDEFINED` 丢弃旧内容），不查不转。

因此 layout 端点是**声明**，不是发现。谁需要就从声明取：
- **Static**：initial/final 烘进 render pass，驱动隐式转换（subpass dependency 同步）。
- **Dynamic**：`vkCmdBeginRendering` 不自动转，需自己发 barrier —— barrier 发射是
  **DynamicRendering 独有**的内部逻辑，用声明的 layout 当 old/new。
- **Attachment 不存可变 current_layout**：两条策略都从声明掌握 layout，存副本只会漂移。

真正需要**可变 per-subresource 追踪**的是"跨 pass 混合使用（这趟当 attachment 写、下趟采样）"，
这是 **RenderGraph 的中央职责**（由 pass 依赖图算转换），key 是 subresource、用区间而非
dense 网格。现无 RenderGraph，不建；届时任何"局部 current layout"归 graph。

## 5. Info 结构体重构：DynamicStructureChain + 混合 API

**动机**：现有 Info 用标量字段 + 手写 `operator vk::X()`，无法带 pNext 扩展链。

**工具**：`utils::DynamicStructureChain<Root>` —— 运行期动态 pNext 链。node-based map
（地址稳定）+ `vk::StructExtends<T,Root>` 编译期校验。**copy-safe**：拷贝后 `relink()` 在
自己的节点上任意序重建整条 pNext（pNext 对 Vulkan 无序）；move 保留地址免处理。类型擦除的
`std::any` 靠 per-node **address thunk**（emplace 时捕获类型）在 relink 时取地址。
`PhysicalDeviceFeatureChain` 已改为组合它。

**混合 API 模式**（每个 Info 持一个 `DynamicStructureChain<vk::XxxCreateInfo>`）：
- **root 常用字段保留干净 setter**（`enableDepthBias` 等捆绑不变量保住）——90% 场景。
- **扩展走 `chain.request<vk::XxxEXT>()` 原始 vk API**——加新扩展零 wrapper 改动。
- `operator vk::X()` 直接 `return chain.root()`，pNext 由 chain 维护。

**分档批量计划**（按结构性质，非一刀切）：
1. **无数组 + 常扩展**（Rasterization / DepthStencil / Tessellation）→ 持 `DynamicStructureChain`。
2. **无数组 + 几乎不扩展**（InputAssembly）→ 直接裸 `vk::XxxCreateInfo`，无 wrapper。
3. **带数组指针**（VertexInput / ColorBlend / Viewport / DynamicState / Multisample /
   Subpass / RenderingInfo）→ **wrapper 保留**（拥有 vector + build 重接指针，chain 只管
   pNext 帮不上数组），仅按需加 chain 成员。

constexpr 放弃：这些 Info 运行期构造，价值近零，且与 chain（`std::any` 非 constexpr）冲突。

## 6. 待做项（按依赖顺序）

1. **`RenderTarget` / `RenderTargetInfo` 落地**：按 §2 的模板+build 形态实现（`RenderTarget.h`
   现为旧模型，需重写为 index 填充 + build 校验生成）。
2. **`Rendering::create` 拼接**：按 §3 组装 `vk::RenderPassCreateInfo2`（authored 直连 +
   resolve 合成 + 联合不变量断言）。中间数组用局部变量，建完即弃。
3. **收掉 `SubpassDescriptionInfo::addResolveAttachment` 手写路径**（§3）。
4. **Info 结构体批量改造**：按 §5 三档推进，Rasterization 作档 1 样板。
5. **StaticRendering framebuffer cache**：cache key = 一组 view 句柄 + extent + layers；
   条目附 `ResourceLease` 钉住 image 存活。
6. **DynamicRendering::begin/end**：从 RT 组装 `VkRenderingAttachmentInfo`（color/resolve/
   depth），发 pre/post layout barrier。

**推迟**：depth/stencil resolve；render pass 缓存（Filament 式，按 format 签名 key，
现用单 render pass 模型 A 足够）；跨 pass layout 追踪（RenderGraph）。
