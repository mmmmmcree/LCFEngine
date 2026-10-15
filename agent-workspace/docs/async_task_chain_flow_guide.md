# 从当前 Chain / Flow 理解 C++ 协程与 Asio

这份文档面向正在阅读本项目代码的你。目标是把“协程、executor、handler、任务节点、并行依赖”放在同一个上下文里，而不是先学习一遍网络编程。

核对日期：2026-10-15。代码依据是当前工作区；本机构建使用 Asio 1.32.0、C++23。接口位于 `lcf::async::task`，对应现有私有头文件中的 `Chain` / `Flow`；互操作用法见 [顺序段、共享边界与合并](async_task_chain_flow_interop.md)。

## 1. 阅读顺序：先回答这些问题

第一次阅读建议按正文顺序。第 2–9 节解释任务怎样运行；第 10–13 节解释结果、生命周期和停止；最后再看互操作设计。

| 知识点 | 你需要能回答的问题 | 当前代码中的对应位置 |
| --- | --- | --- |
| coroutine frame、挂起与恢复 | 等待期间局部变量为什么还存在？线程是否被阻塞？ | Chain 的 `run_node`、Flow 的 `run_flow_node`、公共 `invoke_task` |
| `awaitable` 与 `co_await` | 调用协程是否立即执行？等待子协程是否必然排队？ | Chain 嵌套等待前驱；Flow 的 ready 快速路径 |
| `io_context` 与 executor | 什么地方决定任务在哪里执行？谁提供线程？ | `RenderTaskSystem::run`、两个类的 `link` |
| `co_spawn` 与 completion token | 怎样启动独立执行，怎样等待它结束？ | `start`、`co_spawn(executor, ..., use_awaitable)` |
| `post`、handler、`poll_one` | 为什么处理一个 handler 可能推进多层协程？ | Flow 边界与公共任务调用中的 `post`、`processOne` |
| 先启动、再等待 | 顺序写两个等待为什么仍可并行？ | 互操作层的 `merge_results` |
| 完成通知与发布同步 | 怎么保证只启动一次、多个等待者都收到通知？ | Flow 的 Node、`subscribe`、`publish` |
| 所有权与生命周期 | 谁保证函数捕获、输入结果、协程帧活得足够久？ | `shared_ptr`、`m_operation`、按值协程参数 |
| 结果、异常与停止 | 业务失败、异步异常、停止事件循环有何区别？ | `Result`、`exception_error`、`stop` |

暂时不需要深入掌握网络 socket、自己实现协程内存分配器、所有 completion token，也不需要直接编写 `promise_type`。先能沿着现有任务走一遍，再向底层看。

## 2. 从 RenderSystem 的实际任务开始

入口是 [RenderSystem.cpp](../../examples/shader_toy/render_system/src/RenderSystem.cpp) 中的 `create_cloud_sea_tasks`。

~~~mermaid
flowchart LR
    I["读取 image.png<br/>CPU"] --> U["上传图片<br/>GFX"]
    U --> P["创建 CloudSeaPacket<br/>GFX"]
    S["读取两个 shader 源码<br/>CPU"] --> C["编译两个 shader<br/>CPU"]
    C --> H["创建 ProgramInfo<br/>CPU"]
    P --> M["merge：等待两路"]
    H --> M
    M --> G["prepare_cloud_sea_passes<br/>GFX"]
    G --> R["prepare_instance_info<br/>Render"]
    R --> N["创建 instance / frame context<br/>Render"]
    N --> Q["collectGarbage<br/>GFX"]
~~~

这里同时存在三种顺序：

1. 图片分支内部有数据依赖：先有像素，才能上传，之后才能创建 packet。
2. shader 分支内部有数据依赖：先有源码，才能编译，之后才能整理 program info。
3. 两路相遇时，`merge` 要等 packet 和 program info 都准备好，后继才能执行。

图片分支与 shader 分支没有互相依赖，因此它们可以在时间线上重叠。单条分支仍按顺序执行，即使相邻任务使用不同 executor。

这不代表两个 shader 也在并行编译：目前它们在一个任务内通过循环顺序编译；[ShaderToyResourceTasks.cpp](../../examples/shader_toy/render_system/src/ShaderToyResourceTasks.cpp) 的编译器调用还使用 `compiler_mutex`。这里的分支并行、CPU 线程执行和 GPU 命令执行，要分别理解。

创建 Chain / Flow 和调用 `link` / `merge` / `as_flow` 是建图。实际任务函数在 `start` 后、相应 executor 得到驱动时执行。

## 3. 先区分五种不同的对象

| 对象 | 它保存什么 | 它决定什么 |
| --- | --- | --- |
| `Chain<T>` / `Flow<T>` 句柄 | 节点指针、默认 executor | 如何构建后继、从哪里开始执行 |
| Node | 操作、结果、执行状态；Flow 还有等待者 | 同一任务是否已执行、结果何时可读 |
| coroutine frame（协程帧） | 挂起后仍需使用的参数、局部状态、恢复位置等 | 从哪里继续执行 |
| executor | 执行上下文及执行约束的访问方式 | 可在哪里执行工作 |
| handler | 某次可被 Asio 调用的函数对象，可能恢复一个协程 | 事件循环这一次调用什么 |

**一个 Node 不等于一个 coroutine frame，也不等于一个 handler。**

一个 Flow 节点执行时会涉及等待依赖的协程、工作协程、完成通知等；反过来，一个恢复协程的 handler 也可能连续推进多个协程帧。

Node 的结果缓存与 coroutine frame 的临时局部变量也不同：协程返回后帧可以销毁，仍被句柄持有的节点结果可以继续存在。

## 4. C++ 协程到底保存了什么

普通函数调用返回后，它的局部执行状态通常就结束了。协程允许函数执行到某处暂停，以后从同一个位置继续；编译器为需要跨越挂起点的状态建立协程帧。

例如，下面是说明机制的例子，不是项目里已有的函数：

~~~cpp
asio::awaitable<int> wait_then_add(int input)
{
    asio::steady_timer timer {co_await asio::this_coro::executor};
    timer.expires_after(std::chrono::milliseconds {1});
    co_await timer.async_wait(asio::use_awaitable);
    co_return input + 1;
}
~~~

timer 等待时，`input`、timer 的生命周期以及“下一步要执行 return”的信息仍然有效。执行该协程的线程可以去运行别的 handler；timer 就绪后，协程才继续。

你可以将一个协程看作一份能够恢复的执行状态。不过，协程语法本身没有创建线程，也没有为同步重任务提供自动抢占：如果协程内部正在执行一个 20 ms 的同步编译函数，那 20 ms 仍占用当前执行线程。

底层有几个名字需要认识：

| 名字 | 在这个上下文中的作用 |
| --- | --- |
| `promise_type` | 协程返回对象、结果和异常的协议；它不等同于 `std::promise` |
| `initial_suspend` | 函数体开始之前，是否先挂起 |
| `final_suspend` | 函数结束时，如何交还控制和处理帧生命周期 |
| `await_transform` | promise 对 `co_await` 对象进行适配的机会 |
| `await_ready / await_suspend / await_resume` | 等待是否就绪、如何暂停、恢复后怎样取得结果 |
| `coroutine_handle` | 对协程执行状态的控制入口 |

当前代码不直接实现这些协议，Asio 的 `awaitable` 已经实现了它们。知道这些名字，是为了下一节能解释“挂起”和“事件循环重新调度”为什么不是同一件事。

## 5. 调用 awaitable、co_await awaitable 与启动执行

### 5.1 调用一个 Asio 协程，通常先得到执行对象

本地 Asio 的 [initial_suspend](../../build/gcc-vkc/vcpkg_installed/x64-mingw-dynamic/include/asio/impl/awaitable.hpp#L108) 返回 `suspend_always`。因此：

~~~cpp
auto task = wait_then_add(3);
~~~

这会建立协程执行对象，函数体尚未正常推进。要通过另一个 Asio 协程的 `co_await`，或者通过 `co_spawn`，接入 Asio 的执行机制。

注意：返回类型写着 `asio::awaitable<T>` 的普通函数，也可以只是构造并返回另一个 awaitable。一个函数体是否是协程，要看是否使用 `co_await / co_return / co_yield` 等协程语法，不能只看返回类型。

### 5.2 co_await 的通用含义是等待与取得结果

~~~cpp
auto value = co_await wait_then_add(3);
use(value);
~~~

`use` 依赖前面的结果。等待期间，当前协程可能挂起；等待完成后，再执行 `use`。

但不能从这句话推导出“必然在 io_context 的下一次 poll 才执行 use”。等待对象的具体实现，决定了是否会把控制交回事件循环。

一个直接 `co_return 42` 的子协程，不包含真正需要等待的外部事件。Asio 可以在当前的协程推进过程中完成它，再继续父协程。

### 5.3 Asio 的协程栈解释了这个现象

本地 [awaitable.hpp](../../build/gcc-vkc/vcpkg_installed/x64-mingw-dynamic/include/asio/awaitable.hpp#L95) 中，`awaitable::await_ready()` 返回 false；`await_suspend` 把子协程帧接入父协程的执行栈。

这仍然不等于“放进 io_context 队列”。[push_frame / pop_frame](../../build/gcc-vkc/vcpkg_installed/x64-mingw-dynamic/include/asio/impl/awaitable.hpp#L473) 管理帧之间的关系，[pump](../../build/gcc-vkc/vcpkg_installed/x64-mingw-dynamic/include/asio/impl/awaitable.hpp#L766) 连续恢复栈顶帧，直到没有可继续推进的帧，或执行栈所有权交给异步操作。

`awaitable_thread` 是这套内部执行栈机制的名字，不代表新建一个操作系统线程。

所以，一个 handler 可以触发：

~~~text
恢复父协程 → 进入子协程 → 子协程完成 → 返回父协程
          → 继续下一段逻辑 → 遇到 timer / channel / post 后交出控制
~~~

另外，`co_await asio::this_coro::executor` 只是取得当前协程的 executor。本地实现的这条等待路径直接就绪，不会因此排一次队。

## 6. io_context 与 executor：线程从哪里来

当前 [RenderTaskSystem.cpp](../../examples/shader_toy/render_system/src/RenderTaskSystem.cpp) 的对应关系很具体：

| context / executor | 谁调用事件循环 | 实际线程资源 |
| --- | --- | --- |
| spawn / render context | RenderSystem 的 worker 循环调用 `processOne` | 一个 render worker |
| CPU context | 两个 `jthread` 分别调用同一个 context 的 `run` | 两个 CPU worker |
| GFX context | 一个 `jthread` 调用 context 的 `run` | 一个 GFX worker |

executor 不会自己创造这些线程。真正执行 handler 的，是调用 `run / poll / poll_one` 的线程。

尤其要注意，**CPU executor 对应的是一个由两个线程驱动的 context，不是某一个固定的 CPU 线程**。两个独立任务可以分配到不同 worker；一次异步等待之后，也不要假设必然回到原来的 CPU worker。

当前项目的 render 和 GFX 之所以体现为固定线程，是因为各自只有一个线程驱动对应 context。

## 7. co_spawn：把协调协程和工作协程分开

两个类都使用类似的结构：

~~~cpp
auto result = co_await asio::co_spawn(
    worker_executor,
    invoke_task(function, inputs),
    asio::use_awaitable);
publish(std::move(result).value());
~~~

这段代码有两条执行线：

1. 外层协调协程运行在自己的 executor 上，启动工作并等待结果。
2. 工作协程运行在 `worker_executor` 上，调用实际任务函数。

工作完成后，`use_awaitable` 产生的完成 handler 带着调用协程的关联 executor，外层协程在那里继续。这是本地 `co_spawn` 实现里的 [handler_work](../../build/gcc-vkc/vcpkg_installed/x64-mingw-dynamic/include/asio/impl/co_spawn.hpp#L82) 与工作 executor 分开的原因。

~~~mermaid
sequenceDiagram
    participant R as Render 协调协程
    participant W as CPU / GFX 工作协程
    R->>W: co_spawn(worker_executor, invoke_task)
    Note over R: 等待工作结果，当前协程挂起
    W->>W: 执行函数，或等待函数内部异步操作
    W-->>R: 通过关联 executor 完成
    R->>R: 恢复，publish，再推进依赖关系
~~~

对于当前 RenderSystem 的这一次 `start(render_executor, ...)`，Chain 的嵌套协调逻辑，或 Flow 的节点协调逻辑，使用 render 上下文；每个 `link` 指定的 executor 用于实际工作。

Flow 比 Chain 更灵活：共享节点可以被不同 `start` 同时请求执行。谁先通过 `tryStart`，谁的协调 executor 就启动该节点；其他调用者等待它完成。因此，“所有 run_node 都在 render”是当前实例的执行安排，不是 Flow 类型的普遍保证。

对应代码：[Chain::link](../../examples/shader_toy/render_system/src/private_include/AsyncTaskChain.h)、[Flow::link](../../examples/shader_toy/render_system/src/private_include/AsyncTaskFlow.h)。基础协议可对照 [Asio 的 C++20 协程文档](https://think-async.com/Asio/asio-1.30.2/doc/asio/overview/composition/cpp20_coroutines.html)。

## 8. handler、post 与 poll_one：为什么不能按节点预算一帧

### 8.1 poll_one 的计数单位

当前 [processOne](../../examples/shader_toy/render_system/src/RenderTaskSystem.cpp) 调用 `m_spawn_thread_io_context.poll_one()`，但没有把它的返回计数传出来。

`poll_one` 至多执行一个已就绪的 handler；没有可执行 handler，或 context 已停止时，可以返回 0。它不会等待未来事件变成就绪。但是，一旦开始调用 handler，就要运行到这次调用返回，不能把同步计算在中途抢占。

同一个计数 `handlers=1`，可以对应一次简短通知，也可以对应一个恢复协程并继续运行较多代码的 handler。

区分两句话：

- 一次 `poll_one` 不会把队列里的所有就绪 handler 都调用一遍。
- 它调用的一个 handler，可能沿协程调用关系推进多段已经能够继续的逻辑。

### 8.2 当前 post 在哪里

[run_flow_node](../../examples/shader_toy/render_system/src/private_include/AsyncTaskFlow.h) 和公共 [invoke_task](../../examples/shader_toy/render_system/src/private_include/async_task_common.h) 都先做：

~~~cpp
co_await asio::post(asio::use_awaitable);
~~~

`post` 给当前 executor 提交后续执行机会；它不会在发起调用的当前调用栈里直接调用完成 handler。和可能在合适上下文中直接执行的 `dispatch` 相比，这里形成了明确的让出控制点。

这不是“等下一帧”。在一个持续 `run` 的 worker 上，后续 handler 很快就可能执行；即使有让出控制点，单个任务函数执行多久仍由它自己的计算决定。

Chain 的实际任务也使用公共 invoke_task，因此包含工作入口的 post；Chain 嵌套推进前驱本身没有逐节点 post。Flow 边界则另有 run_flow_node 的 post。仍不能拿节点个数直接推导 handler 数量。

### 8.3 怎样测量才有意义

下面是观测示例，不是对现有源码的修改：

~~~cpp
const auto begin = std::chrono::steady_clock::now();
const auto handlers = ctx.poll_one();
const auto elapsed = std::chrono::steady_clock::now() - begin;
~~~

`handlers` 衡量派发数量；`elapsed` 衡量本次调用占用当前线程的时间。CPU / GFX 在其他线程上处理的工作，不会全部算进 render 的这一次耗时。

如果要控制每帧负载，需要同时考虑调度次数、时间预算和单个同步任务的最大耗时。handler 数量可以作辅助指标，不能独立给出执行时间上限；把重任务拆分或放到合适 worker，才会改变其对 render 的占用。

## 9. Chain 和 Flow：不同的依赖执行方式

### 9.1 Chain：先前驱，再自己

[Chain::link](../../examples/shader_toy/render_system/src/private_include/AsyncTaskChain.h) 创建一个后继节点，它的操作先：

~~~cpp
co_await Self::run_node(input_sp);
~~~

前驱返回成功结果后，再 `co_spawn` 当前工作。末端启动时，控制先沿前驱关系进入更早的节点，再逐步返回到后继。

~~~text
等待 C → 等待 B → 执行 A
                 A 完成 → 执行 B
                            B 完成 → 执行 C
~~~

跨 executor 可以把 A、B、C 放在不同线程完成，但依赖决定了同一条链中它们不会同时执行。

Chain 的 `link` 和 `start` 都消费右值句柄。这种使用约定允许它不用管理多个独立的完成等待者。其 Node 仍保存结果、操作和 pending / running / completed 原子状态，不是完全没有状态。

`Chain<T>::ResultHandle` 是一个保存早先节点所有权的依赖句柄。Chain::getResult() 返回这个句柄，便于后续任务读取同一条链中较早的值；它不是 std::future，也没有独立等待接口。其 getResult() 只读取已完成结果。额外传入这种依赖不会自动开启另一条并行执行线。

### 9.2 Flow：所有依赖先启动，再逐个等待

关键代码是互操作层的 [merge_results](../../examples/shader_toy/render_system/src/private_include/async_task_interop.h)：

~~~cpp
(FlowAccess::start(std::get<Indices>(inputs), executor), ...);
auto results = std::tuple<ResultView<Values>...> {
    co_await std::get<Indices>(inputs).run()...
};
~~~

第一行发起所有依赖；第二行才等待它们。假设图片和 shader 都已启动，协调协程等待图片时，shader 不需要等它，可以继续工作。

~~~text
时间       ────────────────────────────────────>
图片分支     读取 ─ 上传 ─ 创建 packet ─ 完成
shader 分支  读取 ───── 编译 ───── 整理 ─ 完成
merge        等图片完成 ───────── 等 shader 完成 → 后继
~~~

这里等待表达式按顺序出现，但工作已经先被发起。实际能否在不同线程重叠，还取决于 executor 背后的线程资源、任务的锁和数据依赖。

对于共享前驱：

~~~cpp
auto source = as_flow(std::move(source_chain));
auto a = source.link(task_a);
auto b = source.link(task_b);
auto joined = merge(std::move(a), std::move(b));
~~~

`source` 只应执行一次。a、b 分别等同一个 source 成功，然后可以独立推进。只有 `merge` 等多个前驱，但 source 也需要支持多个后继等待自己的完成。

### 9.3 合并结果为什么没有复制整份输入

`MergedValues` 保存每个输入的 ResultView；视图拥有对应结果的所有者。`result_arguments_traits` 从结果中取得只读引用，供后继函数调用。

所以 `merge(std::move(a), std::move(b)).link(fn)` 的 `fn` 收到 a、b 的结果引用，而不是强制把图片等大对象复制进一个新 tuple。嵌套 merge 会展开合并参数，普通任务返回的 tuple 则仍是一个数据值。

如果某分支失败，当前 merge 仍等所有分支结束，再按参数顺序传播遇到的第一个失败。它不会自动取消兄弟分支；后继业务任务不会执行。

## 10. 为什么 Flow 需要通知，而 Chain 的原子状态不够

[Flow::Node](../../examples/shader_toy/render_system/src/private_include/AsyncTaskFlow.h) 用 mutex 保护启动、完成、结果发布和等待者登记。锁只覆盖这些短操作；实际任务以及 `co_await` 都在锁外。

原子状态只能表达“是否完成”，不能让一个已经挂起的协程在状态改变时自动恢复。因此 Flow 用 channel 为每个等待者提供一次完成通知。

[wait_node](../../examples/shader_toy/render_system/src/private_include/AsyncTaskFlow.h) 的过程是：

1. 如果节点已完成，直接返回。
2. 否则，在当前等待协程的 executor 上创建容量为 1 的 `concurrent_channel`。
3. 在节点锁内登记等待者，同时再次检查是否已完成。
4. 登记成功后，通过 `async_receive(use_awaitable)` 等待通知。

这里有两个必须处理的竞态：

| 节点何时完成 | 现有代码怎样处理 |
| --- | --- |
| 第一次检查之后、登记之前 | `subscribe` 在同一把锁下再检查完成状态，发现完成就不登记 |
| 登记之后、真正开始 receive 之前 | 容量为 1 的 channel 缓存通知，之后 receive 仍能收到 |

[publish](../../examples/shader_toy/render_system/src/private_include/AsyncTaskFlow.h) 先在锁内存好结果、标记完成、取出等待者列表，再在锁外发送通知。唤醒与结果发布的先后关系因此明确。

channel 里发送的是“节点完成了”，不是业务结果或业务错误。业务成功与否仍要读取节点的 `Result`。这也解释了为什么失败的节点仍发送成功的通知事件。

等待协程持有自己的 channel，节点只保存 `weak_ptr`，以免节点与等待协程互相持有而形成所有权环。

当前 start 直接使用指定 executor。共享 Node 通过 mutex 保证唯一启动和发布，跨线程完成通知使用 concurrent_channel。不同等待者在自己的 executor 上继续。若另外使用 strand，它保证使用同一个 strand 的 handler 不并发执行，并不指定固定线程；当前接口不额外创建 strand。

channel 的概念可对照 [Asio 官方 Channels 文档](https://think-async.com/Asio/asio-1.36.0/doc/asio/overview/channels.html)；这里的具体行为以本地 1.32.0 实现和上述节点代码为准。

## 11. completion token、expected 与异常：三者各自负责什么

### 11.1 completion token 决定如何接收异步完成

同一异步操作可以以回调方式或协程方式使用：

~~~cpp
timer.async_wait(on_complete);
co_await timer.async_wait(asio::use_awaitable);
~~~

`use_awaitable` 把异步完成适配为可等待操作。`detached` 表示不接收该操作的结果，不是创建或分离一个操作系统线程。当前 Flow 用它启动节点协调协程，再通过自己的节点结果与 channel 进行观察。

completion signature 是“完成时传什么参数”的契约：

| 完成签名 | 用 use_awaitable 等待时的含义 |
| --- | --- |
| `void(std::error_code)` | 成功返回 void；错误通常转为异常 |
| `void(std::error_code, T)` | 成功取得 T；错误通常转为异常 |
| `void(std::exception_ptr, T)` | 取得 T，或重新抛出异常 |
| `void(Result<T>)` | 取得 expected 本身；其中的失败仍是数据 |

要把现有回调 API 接入这套协议，可以使用 `async_initiate`：它将 completion token 转成实际 handler，再交给发起操作的实现。这是理解“把 Chain::start 改造成可等待异步操作”的知识入口。当前 `start` 只接收 completion 函数，并不是直接接受任意 completion token 的通用异步发起函数。

参考 [Asio Completion Tokens](https://think-async.com/Asio/asio-1.30.2/doc/asio/overview/model/completion_tokens.html)；初次阅读掌握这个关系即可，不必马上写自己的 token。

### 11.2 expected 是业务结果约定

Chain / Flow 通过公共适配接受普通值、void、error_code、expected，以及返回这些形式的 awaitable，再统一成 `Result<T>`。

- 普通 T：成功并携带 T。
- void：成功，没有输入给后继的值。
- error_code：非零表示失败，零表示无值成功。
- `Result<T>`：保留它自己的成功或失败状态，并保留内部 T。

`Result<std::error_code>` 的成功值可以是非零错误码，因为它在此处是数据；嵌套 expected 也保留为数据。反过来，`unexpected(std::error_code {})` 仍是 expected 的失败状态，不能只看错误码是否非零。

Chain / Flow 现已统一这套规则，无值结果都使用 void。明确返回 monostate 的普通任务仍提供一个 monostate 值。

### 11.3 公共 optional 包装解决的是 Asio 传输约束

当前 `invoke_task` 返回 `awaitable<optional<Result<T>>>`。成功时 optional 总有值；失败异常由 `co_spawn(..., use_awaitable)` 抛回协调协程，再转成节点失败。

这个 optional 不是“任务可能没有结果”的业务语义。本地 [co_spawn 的异常完成路径](../../build/gcc-vkc/vcpkg_installed/x64-mingw-dynamic/include/asio/impl/co_spawn.hpp#L188) 会构造返回类型的默认值；optional 可以默认构造，从而避免要求 T 本身可以默认构造。

### 11.4 项目里也有真正的异步等待

[prepare_instance_info](../../examples/shader_toy/render_system/src/ShaderToyResourceTasks.cpp) 检查资源是否准备好，未准备好时调用 [wait_for_retry](../../examples/shader_toy/render_system/src/ShaderToyResourceTasks.cpp)。后者使用 `steady_timer::async_wait(use_awaitable)`。

这次等待把当前协程交给 timer 的完成机制，render worker 可以继续处理别的工作。相比之下，读取文件、编译 shader 这类同步函数只是被放到了 worker 上执行，内部并不会因为包在 awaitable 里就自动变成非阻塞函数。

## 12. 生命周期：理解互操作前最重要的一关

协程参数按值传入时，其必要的存储属于协程状态；引用参数指向的对象，仍需要外部所有权保证。保存一个 `const T &` 并不会延长 T 的生命周期。

当前 Flow 的几个所有权关系值得沿代码核对：

| 保存方式 | 保证了什么 |
| --- | --- |
| `run_node` / `run_flow_node` 按值接收 shared_ptr | 节点在执行期间存在 |
| `invoke_task(Function function, Arguments arguments)` | 按值拥有可调用对象和参数 tuple；其中的引用由外层操作的输入所有权保证 |
| 节点的 `m_operation` 保存协程 lambda | lambda 的闭包在其协程操作完成前存在 |
| 操作捕获父节点 shared_ptr | 等待和调用时前驱结果存在 |
| `MergedValues` 保存 ResultView | 合并结果引用有对应的所有者 |
| `output_node_p` 是 raw pointer | 不形成自身持有环；有效性由正在执行的节点所有权保证 |

协程 lambda 是一个特别容易误判的地方：捕获变量属于闭包对象，不会因为函数体含 `co_await` 就自动成为独立、长寿命的帧参数。临时闭包创建 awaitable 后马上销毁，恢复时再访问捕获，就可能访问已经失效的对象。

当前 Flow 的 `m_operation` 在 `co_await node_sp->m_operation()` 返回后才清空，避开了这个问题。安全边界是“操作协程已完成”，不是“已经调用过这个 lambda”。

Chain::run() && 使用“普通入口立即转移所有权 + run_owned 协程按值持有节点”的结构，避免挂起后继续依赖已销毁的临时 this。互操作文档有完整说明。

如果要进一步核对这类问题，可以阅读 [C++ Core Guidelines CP.51](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#cp51-do-not-use-capturing-lambdas-that-are-coroutines) 与 CP.53。它们讨论捕获式协程 lambda 和协程引用参数的生命周期。

## 13. stop、取消和等待完成

当前 `RenderTaskSystem::stop` 停止三个 context，并请求 worker 停止；`join` 等线程退出。它没有给每个任务建立一个“收到取消后发布失败结果”的完整协议。

因此，停止派发 handler，不等同于所有 Flow 都完成并返回取消错误。同步执行中的函数，也不会被 `io_context::stop` 在中途抢占。

`executor_work_guard` 表示仍有工作需要等待，使 `run` 不会仅因当前没有任务就退出；它不启动线程，也不能让已停止的 context 自动恢复。`restart` 用于允许停止后的事件循环重新运行，也不是任务级取消接口。

当前 merge 的“等完兄弟再报告失败”，是正常推进情况下的行为。要增加可取消任务、全图取消或 shutdown 时的结果收尾，需要另外定义完成、取消、资源释放之间的关系。

## 14. 回到互操作：现有知识怎样接起来

现在可以把一条 Chain 当作一份复合异步操作理解：

~~~text
Flow 节点获得执行资格
    → 开始整条 Chain
    → 协程等待 Chain 完成，线程继续做其他工作
    → 发布 Chain 的终值或失败
    → 唤醒等待该 Flow 节点的后继
~~~

Chain 内部继续嵌套推进和跨 executor 执行；Flow 只观察它的整体完成。关键不是把 Chain 的每个节点变成 Flow 节点，而是把“可等待完成、结果所有权、错误传播”这条边界定义清楚。

接下来阅读 [Chain / Flow 互操作设计](async_task_chain_flow_interop.md)。如果还有不清楚的地方，建议先沿这两条代码路径读：

1. Chain：`start → run → run_owned → run_node → m_operation → 前驱 run_node → co_spawn(invoke_task) → publish`。
2. merge / Flow：`start → run_owned → ensure_started → run_flow_node → merge_results → 启动所有输入 → 等待所有输入 → publish → channel 唤醒`。输入是 as_flow 适配的 Chain 时，在边界里等待 Chain.run。

### 自检：能回答这些问题，就具备继续设计的上下文

1. 为什么 `co_await` 不一定意味着 io_context 下次调度？
2. CPU executor 为什么不能等同于某一个固定线程？
3. 同一条链跨三个 executor，为什么仍可能完全顺序执行？
4. `merge_results` 中的顺序等待，为什么没有把已经启动的两路工作串行化？
5. 只有 merge 等多个依赖，为什么共享 source 仍需要完成通知？
6. `handlers=1` 为什么不能保证一帧只运行一个任务节点？
7. 为什么 Chain 完成回调里的 `const Result &` 不能直接保存为长久有效的 Flow 结果？
8. 为什么 stop 之后，不能直接断言所有 awaitable 都已收到取消并完成？
