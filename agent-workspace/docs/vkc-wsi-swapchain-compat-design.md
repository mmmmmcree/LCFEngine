# WSI Swapchain 兼容层（不依赖 swapchain_maintenance1）

> 目标：在不支持 `VK_EXT/KHR_swapchain_maintenance1` 的设备上，提供与
> `vkc::wsi::Swapchain` **接口一致**的实现，去掉 present fence。
> 本文只给设计与实现要点，不复制代码。

---

## 1. 命名空间约定（跨模块统一）

```
vkc::wsi::Swapchain          // 主实现：用 maintenance1，present fence 回收
vkc::wsi::compat::Swapchain  // 兼容实现：接口一致，不用 present fence
```

**约定**：任何模块需要"功能等价、但绕开某个可选扩展/特性"的退化实现时，
统一放进该模块命名空间下的 `compat` 子命名空间。
- 接口（公开方法签名）必须与主实现逐字一致 —— 这样调用方可用别名切换：
  ```cpp
  namespace app { using Swapchain = vkc::wsi::Swapchain; }        // 或 compat::
  ```
- `compat` 只允许"为兼容性做的退化"，不引入新功能。功能差异只能体现在**内部机制**，不能体现在接口。

> 为什么不是模板/虚基类：模板会把"选哪个实现"推到编译期、污染所有使用点；
> 虚基类引入运行时多态开销且要求统一抽象。两个独立类型 + `using` 别名最轻，
> 切换点收敛在一处（创建时按 feature 是否启用决定别名指向谁）。

---

## 2. 核心差异：回收触发器与 present_ready 的归属

### 2.1 maintenance1 版（现状回顾）
`SwapchainPresentFenceInfoEXT` 给 `vkQueuePresentKHR` 挂一个 fence，
**present 完成**时 signal。`tryRecycle` 轮询这个 fence，完成即回收整批
`PresentResources`（cmd / target_available / present_ready / fence）。
关键：present fence 精确表示"presentKHR 这次操作彻底用完了这批资源"。

### 2.2 compat 版没有 present fence，怎么办？

**能拿到的替代触发器**：blit submit 的 fence（`vkQueueSubmit(..., fence)`）。
它表示"**blit command buffer 执行完毕**"。

但 submit fence 只覆盖了 blit 的完成，**没覆盖 present 的完成**。这带来两类资源、两种生命周期：

| 资源 | 何时可安全回收 | submit fence 够吗 |
|---|---|---|
| `m_cmd`（blit cmd buffer） | blit 执行完 | ✅ 够 |
| `m_target_available`（acquire 等的 semaphore） | blit 开始执行（被 wait 后） | ✅ 够（submit 完成 ⇒ 必已 wait 过它） |
| **`m_present_ready`**（presentKHR wait 的 semaphore） | **present 完成**，不是 blit 完成 | ❌ **不够** |

**问题核心**：`m_present_ready` 由 blit submit signal、由 `presentKHR` wait。
blit submit fence signal 时，blit 做完了、semaphore 已 signal，但
**presentKHR 可能还在等它/还没消费完**。此时若把 `m_present_ready` 还池、
下一帧重用，就会出现"一个 binary semaphore 同时被两次 present 等待" →
validation 报错 / 未定义行为。

没有 present fence，**你无法知道 presentKHR 何时真正用完 present_ready**。

### 2.3 经典解法：present_ready 按 image index 持有，靠"再次 acquire 同一张"隐式同步

maintenance1 之前所有引擎的标准做法：

> **每个 swapchain image index 配一个固定的 `present_ready` semaphore，
> 不进回收池、不跨 index 复用。**

原理（关键不变量）：
- 你只有在**再次 acquire 到同一个 image index** 时，才会重用它的 present_ready。
- 而能再次 acquire 到 image i，**意味着呈现引擎已经把 image i 用完归还**了
  （否则 acquire 不会把这个 index 给你）。
- image i 上一次的 present 早已完成 ⇒ 它的 present_ready 早已被消费完 ⇒ 重用安全。

这就是用"acquire 同一 index"这个事件，**替代**了 present fence 的"present 完成"信号。
不需要任何额外 fence。

### 2.4 结论
compat 版的资源拆成两组：
1. **随 blit submit fence 回收的**：`m_cmd`、`m_target_available`、`m_leases`、以及 submit fence 本身。
2. **按 image index 长期持有、永不进回收池的**：`present_ready[image_count]`。

---

## 3. 成员与结构调整（相对主实现）

```cpp
namespace lcf::vkc::wsi::compat {

class Swapchain
{
    // 接口区与主实现逐字一致（create / 三个 present 重载 / 私有 recreate 等）

    struct PresentResources              // 只装"随 submit fence 回收"的部分
    {
        vk::CommandBuffer m_cmd = nullptr;
        vk::UniqueSemaphore m_target_available;
        vk::UniqueFence m_submit_fence;  // ← 原 m_present_fence 改名：现在是 blit submit fence
        std::vector<ResourceLease> m_leases;
        // 注意：不再持有 m_present_ready
    };

private:
    // present_ready 按 image index 固定持有，随 swapchain 重建而重建
    std::vector<vk::UniqueSemaphore> m_present_ready_semaphores;   // size == m_swapchain_images.size()

    // 其余成员同主实现：m_pending_resources_queue / m_cmd_buffer_pool /
    // m_fence_pool / m_semaphore_pool（仅供 target_available 复用）/ m_cmd_pool ...
};

}
```

---

## 4. 方法实现要点

### 4.1 `create`
- 与主实现一致，**但不校验/不依赖 swapchainMaintenance1 feature**。
- 同样调 `getSurfaceSupportKHR`，建 cmd pool。

### 4.2 `recreate`
- 与主实现一致地重建 swapchain + 取 image 数组。
- **额外**：image 数组大小确定后，重建 `m_present_ready_semaphores`：
  ```cpp
  m_present_ready_semaphores.clear();          // 旧的随 UniqueSemaphore 析构
  m_present_ready_semaphores.resize(m_swapchain_images.size());
  for (auto & sem : m_present_ready_semaphores) { sem = device.createSemaphoreUnique({}); }
  ```
  > 重建时旧 present_ready 直接析构是安全的：recreate 由 OutOfDate 触发，
  > 此处调用方应已确保旧 swapchain 的 present 不再进行（或在 destroy 前 waitIdle）。
  > 若要更严格，可把旧 vector move 进一个"待 device.waitIdle 后清理"的暂存，
  > 但 compat 版按"重建即弃旧"处理即可，与主实现的旧资源处理风格一致。
- `surface_zero_size` 早退逻辑不变。

### 4.3 `acquireNextImage`
- 与主实现一致：用池子里的 `target_available` semaphore 调 `acquireNextImageKHR`，
  填入 `m_present_resources.m_target_available`。
- OutOfDate → recreate → 递归，逻辑不变。

### 4.4 `present`（核心差异）
顺序：
1. 前置检查 / acquire（同主实现，得到 `m_image_index`）。
2. 取 `m_cmd`、取 **submit fence**（从 `m_fence_pool`），填 `m_present_resources`。
   **present_ready 不从池取**，而是用 `m_present_ready_semaphores[m_image_index]`。
3. 录制 blit（barrier → blitImage → barrier），同主实现。
4. **submit 时挂 submit fence**：
   ```cpp
   queue.submit(submit_info, m_present_resources.m_submit_fence.get());
   //   wait   = m_target_available
   //   signal = m_present_ready_semaphores[m_image_index]   ← 按 index 取
   ```
   （主实现是 `submit(submit_info)` 不挂 fence；compat 必须挂，这是唯一的回收触发器。）
5. **presentKHR 不挂任何 fence**（去掉 `SwapchainPresentFenceInfoEXT` 整条 pNext）：
   ```cpp
   vk::PresentInfoKHR present_info;     // 不再用 StructureChain
   present_info.setWaitSemaphores(m_present_ready_semaphores[m_image_index].get())
               .setSwapchains(m_swapchain.get())
               .setPImageIndices(&m_image_index);
   ```
6. OutOfDate 视为 success（你已确认的策略），sentinel = eSuccess。
7. 当前批入 `m_pending_resources_queue`，`tryRecyclePendingResources()`。

### 4.5 `tryRecyclePendingResources`（按 submit fence 轮询）
```cpp
while (not m_pending_resources_queue.empty()) {
    auto & res = m_pending_resources_queue.front();
    if (device.getFenceStatus(res.m_submit_fence.get()) != vk::Result::eSuccess) { break; }
    device.resetFences(res.m_submit_fence.get());
    m_fence_pool.emplace(std::exchange(res.m_submit_fence, {}));
    m_semaphore_pool.emplace(std::exchange(res.m_target_available, {}));  // target_available 回池
    res.m_cmd.reset();
    m_cmd_buffer_pool.emplace(std::exchange(res.m_cmd, nullptr));
    res.m_leases.clear();
    // 注意：这里没有 present_ready —— 它不归本队列管，按 index 常驻
    m_pending_resources_queue.pop();
}
```

---

## 5. 这套方案的正确性边界（务必记住）

1. **`m_present_ready_semaphores` 按 index 复用的安全性**完全依赖
   "再次 acquire 到同一 index ⇒ 该 index 上次 present 已完成"这一隐式同步。
   这是 Vulkan 呈现模型的保证，无需额外 fence。**但前提是该 index 的
   present_ready 只被那一个 index 使用** —— 不要把它放进任何共享池。

2. **销毁顺序**：compat 版析构前同样需要上层 `device.waitIdle()`
   （example 已有），保证 in-flight 的 submit/present 全完成，
   `m_present_ready_semaphores` 与 pending 队列里的 Unique 句柄析构才安全。

3. **target_available 仍走池**：它在 submit 完成后即可回收（submit fence 覆盖），
   与 present 完成无关，所以照旧进 `m_semaphore_pool`。

4. **delayed-recycle 的 FIFO + break 不变量**仍成立：submit 提交序 ≈
   submit fence 完成序，队首未完成即可停。

---

## 6. entry.cpp 的配合（可选扩展注册）

当前 `register_swapchain` 把 `KHRSwapchainMaintenance1` + feature **无条件 required**。
要支持 compat 路径，需把它改成**可选**：
- 提供一个开关（运行时探测设备是否支持，或编译期/配置选项），
  决定走主实现还是 `compat`。
- maintenance1 的扩展/feature 仅在主路径 required；compat 路径只 required
  `KHRSwapchainExtensionName`。
- 选择别名指向的逻辑收敛在创建处：探测 `swapchainMaintenance1` 是否可用 →
  `using ActiveSwapchain = 支持 ? wsi::Swapchain : wsi::compat::Swapchain;`
  （或工厂返回二者的公共包装，取决于你后续是否要运行时切换）。

> entry.cpp 现有的 `//todo optional` / `// if constexpr xxx` 注释正是这个切换点，
> 兼容层落地时一并处理。

---

## 7. 对"统一 compat 约定"的推广

同一套模式适用于其它有"可选扩展增强"的模块：
- 主实现用扩展拿到更优机制（更精确的 fence / 更少同步）。
- `<module>::compat::` 提供接口一致、退化到核心规范的版本。
- 切换收敛在创建/注册处，调用方通过 `using` 别名无感知。
- compat 只退化机制、不增删接口；差异写进各自模块的兼容层文档。
