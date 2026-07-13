# Swapchain → Queue 抽象迁移设计

> 背景：005 已按理想接口改写，Swapchain.create() 尚未适配。
> 目标提交：c469e9a 的编译态。

---

## 1. 目标接口（005 已写出）

```cpp
swapchain.create(
    std::move(unique_surface),           // vk::UniqueSurfaceKHR，移交所有权
    device_context.getPhysicalDevice(),  // vk::PhysicalDevice
    logical_present_queue);              // const LogicalQueue &
```

---

## 2. 成员变更

| 旧成员 | 状态 | 替代 |
| --- | --- | --- |
| `vk::Queue m_present_queue` | 删除 | `LogicalQueue m_logical_queue` |
| `TimelineSemaphore m_blit_timeline` | 删除 | `Queue m_queue`（内含 timeline） |
| `vk::UniqueCommandPool m_cmd_pool` | 删除 | `Queue m_queue`（内含 cmd allocator） |
| `CmdBufferPool m_cmd_buffer_pool` | 删除 | `Queue m_queue`（内含 cmd allocator） |
| `vk::PhysicalDevice m_physical_device` | 保留 | — |
| `vk::Device m_device` | 保留，由 `logical_queue.getDevice()` 填充 | — |
| `vk::UniqueSurfaceKHR m_surface` | 保留，改为移入而非内部创建 | — |
| `m_fence_pool`, `m_semaphore_pool` | 保留 | present 流特有（binary sema + fence），Queue 不管 |
| `m_pending_resources_queue` | 保留 | fence-based 回收，Queue 不替代 |
| `m_present_mutex`, `m_resize_has_priority` | 保留 | 渲染线程/resize 线程串行化 |

新增两个成员：
```cpp
Queue      m_queue;         // 拥有：timeline + cmd allocator + lease 回收
LogicalQueue m_logical_queue; // 轻量 handle，用于 acquireAccess()
```

---

## 3. Queue 需要新增一个方法

`_present()` 里的 blit submit 需要同时 signal：
- binary semaphore（`present_ready`，用于 `presentKHR` 等待）
- timeline semaphore（供调用方的下一帧 wait）

`Queue::submit(CommandBufferBatch &&)` 目前只管 timeline signal，无法附加 binary signal。因此 blit submit 必须走 `QueueAccess` + raw `submit2()`，Queue 只负责 timeline 推进。

需要在 `Queue` 上暴露一个方法：

```cpp
// Queue.h 新增
// 推进 timeline target，返回 signal submit info（供调用方自行放入 SubmitInfo2）
vk::SemaphoreSubmitInfo advanceTimeline() noexcept;
```

实现（一行）：
```cpp
vk::SemaphoreSubmitInfo Queue::advanceTimeline() noexcept
{
    return m_timeline.advanceTarget().generateSubmitInfo();
}
```

`Queue::collectGarbage()` 已有，Swapchain 在 `tryRecyclePendingResources()` 里顺带调用即可（或每帧末调用）。

---

## 4. create() 改写

```cpp
std::error_code Swapchain::create(
    vk::UniqueSurfaceKHR surface,
    vk::PhysicalDevice physical_device,
    const LogicalQueue & logical_queue) noexcept
{
    m_physical_device = physical_device;
    m_device = logical_queue.getDevice();
    m_logical_queue = logical_queue;
    m_surface = std::move(surface);

    // 验证 surface support（family index 从 logical_queue 取，不再外部传入）
    bool supported = false;
    try {
        supported = m_physical_device.getSurfaceSupportKHR(
            logical_queue.getFamilyIndex(), m_surface.get());
    } catch (const vk::SystemError & e) { return e.code(); }
    if (not supported) { return errc::no_suitable_present_queue_family; }

    // Queue 一步初始化：timeline + cmd allocator（替代原来的 m_blit_timeline.create +
    // createCommandPoolUnique 两步）
    return m_queue.create(logical_queue);
}
```

删除的旧逻辑：
- `create_surface(instance, window_handle)` — surface 已由调用方创建并移入；
- `m_blit_timeline.create(device)` — 由 `m_queue.create()` 覆盖；
- `m_cmd_pool = device.createCommandPoolUnique(...)` — 由 `m_queue.create()` 覆盖。

---

## 5. _present() 改写要点

### 5.1 acquireCmdBuffer() → allocateCommandBufferBatch()

旧：`vk::CommandBuffer cmd = acquireCmdBuffer()`（从内部 pool 取裸 handle）  
新：`auto batch = m_queue.allocateCommandBufferBatch(info)` — 批次持有 cmd，回收由 Queue 的 timeline 驱动。

录制方式随之变化：通过 `CommandBufferProxy cmd = batch.getProxy(0)` 录制（与 005 渲染线程写法一致）。

### 5.2 blit submit

旧：
```cpp
vk::Queue present_queue = m_present_queue;
present_queue.submit2(submit);
```

新（两步，保持原有 binary + timeline 双 signal）：
```cpp
vk::SemaphoreSubmitInfo blit_timeline_signal = m_queue.advanceTimeline();
// 把 blit_timeline_signal 加入 submit 的 signals 数组
auto access = m_logical_queue.acquireAccess();
access.getQueue().submit2(submit);   // submit 仍包含 present_ready binary signal
```

`m_pending_resources_queue` / fence 回收路径不变。

### 5.3 presentKHR

旧：`present_queue.presentKHR(...)`  
新：`access.getQueue().presentKHR(...)` — 与 submit2 复用同一个 `QueueAccess`，一次锁内完成，语义更强。

```cpp
// submit2 + presentKHR 在同一 access 范围内
auto access = m_logical_queue.acquireAccess();
access.getQueue().submit2(submit);
vk::Result present_result = access.getQueue().presentKHR(...);
```

### 5.4 recyclePresentResources() 中的命令缓冲回收

旧：`m_cmd_buffer_pool.emplace(cmd)` + `cmd.reset()`  
新：`batch` 由 `m_queue` 的 timeline 回收（`collectGarbage()` 驱动）；`PresentResources` 里存 `CommandBufferBatch` 而非裸 `vk::CommandBuffer`。

`tryRecyclePendingResources()` 末尾加一行：`m_queue.collectGarbage()`。

### 5.5 acquireCmdBuffer() 方法可删除

整个 `acquireCmdBuffer()` 私有方法及其对应的 `m_cmd_pool` / `m_cmd_buffer_pool` 成员随之删除。

---

## 6. 头文件变更

`Swapchain.h` 新增 include：
```cpp
#include "vk_core/queue/Queue.h"
#include "vk_core/queue/LogicalQueue.h"
```

删除（不再直接使用）：
```cpp
#include "vk_core/sync/TimelineSemaphore.h"   // TimelineSemaphore 已封入 Queue
```

`Swapchain.cpp` 删除 `#include "vk_core/WSI/create_surface.h"` 和
`#include "vk_core/WSI/WindowHandle.h"`（surface 由调用方创建，Swapchain 不再需要）。

---

## 7. 变更总结

| 方面 | 旧 | 新 |
| --- | --- | --- |
| create() 参数 | instance + device + family_index + queue + window | UniqueSurface + PhysicalDevice + LogicalQueue |
| surface 创建 | Swapchain 内部 | 调用方创建，移交 |
| timeline 管理 | `m_blit_timeline` | `m_queue`（`advanceTimeline()` 暴露信号） |
| cmd 分配 | `m_cmd_pool` + `m_cmd_buffer_pool` | `m_queue.allocateCommandBufferBatch()` |
| queue host access | 裸 `m_present_queue`（vk::Queue）| `m_logical_queue.acquireAccess()` |
| submit + present | 两次裸 queue 调用 | 同一 QueueAccess 范围内 |
| cmd 回收 | fence-based 手动 | timeline-based via `m_queue.collectGarbage()` |
| Queue 新增 API | — | `Queue::advanceTimeline()` |
