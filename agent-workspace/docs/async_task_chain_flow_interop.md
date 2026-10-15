# Chain / Flow：顺序段、共享边界与合并

本文对应 2026-10-15 的实现。代码仍在 shader_toy 的 private_include 中验证，命名空间是 `lcf::async::task`，尚未独立成库。

如果想先理解协程、executor、handler 与完成通知，可以先看 [C++ 协程与 Asio 导读](async_task_chain_flow_guide.md)。本文先讲怎么用，再解释所有权和调度。

## 1. 用三个操作表达依赖

~~~cpp
#include "async_task_interop.h"

namespace task = lcf::async::task;

auto image_chain = task::Chain {image_path, cpu_executor}
    .link(load_image)
    .link(gfx_executor, upload_image);

auto shader_chain = task::Chain {shader_paths, cpu_executor}
    .link(load_sources)
    .link(compile_sources);

auto tasks = task::merge(std::move(image_chain), std::move(shader_chain))
    .link(gfx_executor, prepare_passes)
    .link(render_executor, create_instance);

std::move(tasks).start(render_executor, on_complete);
~~~

这是接口形状示例，任务函数的签名需要与实际输入匹配。当前项目的完整调用见 [RenderSystem.cpp](../../examples/shader_toy/render_system/src/RenderSystem.cpp)。

只需先记住：

| 操作 | 接收什么 | 返回什么 | 所有权 |
| --- | --- | --- | --- |
| `Chain::link` | 下一个函数，可指定工作 executor | 新的 Chain | 消费原 Chain |
| `Flow::link` | 下一个函数，可指定工作 executor | 新的 Chain | 保留、共享原 Flow |
| `merge(a, b, ...)` | 至少两个 Chain / Flow，可混用 | Flow | 消费 Chain；复制或移动 Flow 句柄 |
| `as_flow(chain)` | 一条 Chain | Flow | 消费 Chain，增加一个共享完成边界 |

**所有 link 返回 Chain；merge 返回 Flow；as_flow 把一整条 Chain 变成可共享的 Flow。**

创建 Chain / Flow、调用 link / merge / as_flow 都是在构建执行关系。实际任务函数要等 start 或 co_await run 接入执行后才运行。

## 2. 为什么 link 后回到 Chain

顺序段只需要“等前驱完成，再执行自己”，不需要为每个 link 保存多个等待者。

~~~mermaid
flowchart LR
    A["Chain A<br/>读取 → 上传 → packet"] --> M["merge<br/>共享 Flow 边界"]
    B["Chain B<br/>读取 → 编译 → program info"] --> M
    M --> C["Chain C<br/>准备 pass → 创建 instance"]
~~~

Chain 节点保存结果、操作和原子状态。Flow 节点额外保存 mutex、等待者列表和共享完成状态。现在在顺序段使用 Chain，在汇合或显式共享的位置使用 Flow。

这由接口的返回类型直接表达，不需要在运行前扫描拓扑、分析哪些中间节点可融合。Node 仍嵌套在各自类内，没有虚基类。

当前 RenderSystem 就采用这张图：图片和 shader 分别是 Chain，merge 等两路结束，之后的顺序工作又是一条 Chain。图片和 shader 可以重叠；两个 shader 仍在同一个任务中的循环里顺序处理。

## 3. 分叉：在哪个位置显式共享

如果一个顺序段的结果要交给多个后继，先用 as_flow：

~~~cpp
auto source = task::as_flow(
    task::Chain {path, cpu_executor}
        .link(load_image),
    cpu_executor);

auto a = source.link(cpu_executor, analyze_image);
auto b = source.link(gfx_executor, upload_image);

auto joined = task::merge(std::move(a), std::move(b))
    .link(render_executor, use_both_results);
~~~

`source` 是可复制的 Flow；a 和 b 是各自独占的 Chain。即使两边被分别 start，load_image 也只执行一次，每边等待自己的 source 结果后继续。

如果 a 自己还要被多个地方使用，也应显式共享 a 的整个顺序段：

~~~cpp
auto shared_a = task::as_flow(std::move(a));
auto a1 = shared_a.link(task_a1);
auto a2 = shared_a.link(task_a2);
~~~

不能复制 Chain，也不能对 Chain 左值调用 link。它的所有权只沿一个方向交给后继、start、run 或 as_flow。需要分叉时才建立共享边界。

`as_flow(chain)` 继承 Chain 的默认工作 executor；`as_flow(chain, executor)` 指定返回 Flow 的默认工作 executor。这个参数不决定整条 Chain 的协调线程，也不覆盖已有 link 指定的工作 executor。

## 4. merge：先启动所有分支，再等待

merge 接受混合输入，例如：

~~~cpp
auto combined = task::merge(std::move(image_chain), shared_shader_flow);
~~~

内部把 Chain 各自适配成一个 Flow 完成边界，然后建立汇合节点。Chain 内部仍按自己的顺序执行，没有把每个 link 改成 Flow 节点。

[merge_results](../../examples/shader_toy/render_system/src/private_include/async_task_interop.h) 的关键顺序是：

~~~cpp
(FlowAccess::start(std::get<Indices>(inputs), executor), ...);
auto results = std::tuple<ResultView<Values>...> {
    co_await std::get<Indices>(inputs).run()...
};
~~~

先请求启动所有输入，再按参数顺序收集结果。因此，等图片时，shader 已经在推进。真正能否同时占用不同 CPU 线程，取决于 executor 背后的线程数量和任务自身的锁。

同一 Flow 可以重复出现在 merge 中，只执行一次，但会按出现次数提供参数：

~~~cpp
task::merge(source, source)
    .link([](const int & a, const int & b) { return a + b; });
~~~

嵌套 merge 的输入按从左到右的顺序递归展开；void 不提供参数。普通任务返回的 tuple 则作为一个完整值，不会展开。

merge 的默认工作 executor 继承第一个输入；之后可以通过显式 link executor 覆盖。

## 5. 返回值和错误已经统一

公共定义在 [async_task_common.h](../../examples/shader_toy/render_system/src/private_include/async_task_common.h)：

~~~cpp
template <typename T>
using Result = std::expected<T, std::error_code>;

template <typename T>
using ResultView = std::shared_ptr<const Result<T>>;
~~~

Chain / Flow 使用同一组任务调用、结果转换与 concepts：

| 任务函数返回 | 节点结果 | 下一个函数的输入 |
| --- | --- | --- |
| 普通 T | Result<T> 成功 | const T & |
| void | Result<void> 成功 | 无参数 |
| error_code | 非零失败；零为无值成功 | 成功时无参数 |
| Result<T> | 原样保留成功 / 失败及 T | 成功时 const T & |
| asio::awaitable<上述类型> | 等待后按同一规则处理 | 同上 |

这里的 awaitable 使用默认的 `asio::any_io_executor`。调用函数时使用可调用对象的左值，以及输入的 const 引用；支持 move-only 闭包和非默认构造、move-only 结果。

只解释最外层 Result。比如 `Result<Result<int>>` 的内部 expected 是数据；`Result<std::error_code>` 中成功携带的非零错误码也是数据。反过来，`unexpected(std::error_code {})` 仍是失败，不能用错误码是否非零代替 expected 的标签。

失败时：

1. Chain 后续业务函数跳过，向外传播原错误。
2. merge 等所有分支完成后，按参数顺序报告第一个失败。
3. `std::system_error` 保留 code；其他任务异常转换为 io_error。
4. 完成回调接收 `const Result<T> &`，自身抛出的异常不会再被当作任务失败转换。

旧 Chain 的 monostate 无值占位已统一为真正的 void。普通任务如果明确返回 monostate，它仍是一个有值类型。

concepts 在编译期限制任务参数和返回类型、完成回调以及 merge 的输入种类。Chain 左值、错误参数签名等会在接口约束处被拒绝；已消费句柄的重复操作则在运行时抛出 logic_error。

## 6. run：等待完成，并持有终值

除了 start 回调，两类都能提供 awaitable：

~~~cpp
auto result_sp = co_await std::move(chain).run();
auto shared_result_sp = co_await flow.run();
~~~

Chain 的 run 消费句柄；Flow 的 run 保留共享句柄。两者都返回 ResultView，既能读取 expected，也持有结果所有者。

Chain 的入口是返回 awaitable 的普通函数：

~~~cpp
asio::awaitable<ResultView<T>> run() &&
{
    this->requireValid();
    return run_owned(std::move(m_node_sp));
}
~~~

调用入口时立即移走节点，随后临时 Chain 销毁也没有影响。真正的 run_owned 协程按值持有 shared_ptr：

~~~cpp
co_await run_node(node_sp);
const auto * result_p = &node_sp->result();
co_return ResultView<T> {std::move(node_sp), result_p};
~~~

shared_ptr 的别名构造让控制块拥有终点节点，读取指针指向节点内部的 expected。因此不会复制 T，也不会把 T 从原缓存移走。

~~~text
ResultView<T>
    ├─ 所有权 → Chain 终点节点
    └─ 读取指针 → 该节点中的 Result<T>
~~~

这也是 as_flow 的实现基础：等待 Chain.run，把得到的视图发布给 Flow。Flow 节点用 variant 保存“自己的 Result”或“外部 ResultView”，对外都提供 const Result<T> &。

Chain / Flow 的操作闭包都在其协程执行结束后清空，释放不再需要的前驱和捕获。显式保存的结果句柄、MergedValues 中的视图仍会保留相应结果。

## 7. getResult 的两种含义

为了保留 Chain 的早期结果依赖能力，两类的读取接口存在一个区别：

| 接口 | 返回 |
| --- | --- |
| Chain<T>::getResult() | Chain<T>::ResultHandle，可以在任务执行前取得 |
| ResultHandle::getResult() | 已完成的 const Result<T> &，未完成时抛出 logic_error |
| Flow::getResult() | 已完成的 const Result<T> &，未完成时抛出 logic_error |

ResultHandle 嵌套在 Chain 的 public 区域，直接保存对应的节点指针，成员函数在类内实现。link 直接推导句柄类型，并通过 value_type 获取依赖值类型；concept 会检查它确实是相应 Chain 的 ResultHandle。

Chain 的句柄可以作为后面 link 的额外参数：

~~~cpp
auto first = task::Chain {input, cpu_executor}.link(step_a);
auto earlier = first.getResult();
auto tail = std::move(first)
    .link(step_b)
    .link([](const B & current, const A & previous) {
        return combine(current, previous);
    }, earlier);
~~~

这个 handle 只引用同一顺序链里较早的节点，不负责启动或等待独立工作。独立分支通过 merge 合并。void 句柄不提供函数参数。

as_flow 保留这份缓存；即使终值是 unique_ptr，已有 handle 也不会因为适配而看到被搬空的值。

返回的 const 引用自身不延长生命周期。需要跨越句柄生命周期持有结果时，用 run 返回的 ResultView，或保留 ResultHandle / Flow 所有者。

## 8. 哪个 executor 做什么

有两个需要分开的选择：

- link executor：实际调用任务函数的地方。
- start / 外层协程 executor：推进协调逻辑、等待结果，以及执行 start 完成回调的地方。

Chain 的嵌套等待继承协调 executor。Flow 的共享节点由第一个成功 tryStart 的请求启动；其他请求在各自执行上下文等待完成。as_flow 不重写 Chain 的任何工作 executor。

当前 RenderSystem 从 render executor start，所以协调通常由 render 推进，实际工作分给 CPU、GFX 或 render。CPU context 有两个 worker，GFX 和 render 各一个。

这套接口仍不保证“一次 poll_one 只推进一个业务节点”，也不提供每帧硬时间预算。一次 handler 可以推进多段协程；任务中的同步计算也不能被抢占。详见导读的 poll_one 一节。

## 9. 再次执行同一种任务：使用工厂

Chain 是一次性执行对象，Flow 是一次执行的共享缓存；复制 Flow 不会重新运行任务。当前没有“重复 start 时复制整个原型”的协议。

用普通工厂函数为每次输入创建新执行对象：

~~~cpp
auto make_tasks = [&](Input input) {
    return task::Chain {std::move(input), cpu_executor}
        .link(step_a)
        .link(gfx_executor, step_b);
};

auto first = make_tasks(first_input);
auto second = make_tasks(second_input);
~~~

每次调用都有独立节点、结果和执行状态。可变捕获、引用和底层资源仍需按各自实际共享关系管理。

## 10. 文件与验证范围

当前没有新增独立库、模块 CMake 或依赖清单：

| 文件 | 职责 |
| --- | --- |
| [async_task_common.h](../../examples/shader_toy/render_system/src/private_include/async_task_common.h) | Result、ResultView、State、traits、concepts、任务调用和异常转换 |
| [AsyncTaskChain.h](../../examples/shader_toy/render_system/src/private_include/AsyncTaskChain.h) | Chain、ResultHandle、顺序执行与 run |
| [AsyncTaskFlow.h](../../examples/shader_toy/render_system/src/private_include/AsyncTaskFlow.h) | Flow、共享完成通知、link 返回 Chain |
| [async_task_interop.h](../../examples/shader_toy/render_system/src/private_include/async_task_interop.h) | 独立 as_flow / merge，混合输入适配 |

AsyncTaskFlow.h 包含 AsyncTaskChain.h，是因为其 link 直接构造 Chain；Chain 不依赖 Flow 的定义。互操作入口集中在 async_task_interop.h。

验证使用构建目录中的临时程序，没有向 shader_toy 添加测试文件或测试目标。覆盖了并行分支、200 轮跨 executor 共享启动、嵌套 / 重复 merge、失败后等待兄弟分支、void / awaitable、move-only 结果与闭包、原结果句柄保留、临时 Chain.run 的生命周期、完成后释放捕获，以及正反例接口约束。RenderSystem 构建通过。

停止 context 仍只是停止事件派发；尚未加入全图取消及 shutdown 时统一完成所有结果的协议。正常运行时的失败传播和停止后的结果收尾是两件不同的事。
