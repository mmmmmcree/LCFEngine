# stdexec async

独立的 stdexec 实现，公开任务接口仍位于 `lcf::async::task`。它与 Asio 实现共享使用方式和接口测试；运行时、等待通知、timer 和取消实现均不依赖 Asio。

`Chain` 是一次性执行对象；`link()` 消费前一条链，`getResultToken()` 提供不可读取结果的依赖标识。`merge(tokens...)` 只描述依赖，不启动源链；它返回可重复用于分支构造的 `ChainJunction`，每次 `link(executor, function)` 生成独立的后继链。源链和后继链分别 `start()`。完整示例见 [sandbox](tests/sandbox/sandbox.cpp)。

## 控制线程与 worker

```cpp
namespace task = lcf::async::task;
namespace core = lcf::async;

core::Context control;
core::Context worker;
std::jthread worker_thread {[&] { worker.run(); }};

bool done = false;
auto chain = task::Chain {40, worker.getExecutor()}
    .link([](const int & input) { return input + 2; });
std::move(chain).start(control.getExecutor(), [&](const task::Result<int> & result) {
    done = true;
    // Keep this control-thread completion small.
});

while (not done) {
    control.poll({.m_max_callbacks = 32, .m_max_duration = std::chrono::microseconds {200}});
    std::this_thread::yield();
}
control.runUntilIdle();
worker.stop();
```

这里 `start()` 的 executor 负责链的控制流程和最终 completion，`link()` 的 executor 负责实际调用任务。多个 worker 线程可以共同运行一个 worker context；每帧只由帧线程推进 control context。

示例循环用于演示预算接口；引擎中应在每帧确定的阶段调用一次 `control.poll(budget)`，下一帧继续处理剩余工作。

## 协作式预算

| 接口 | 含义 |
| --- | --- |
| `pollOne()` | 非阻塞地取出并执行至多一个队列回调，返回 0 或 1 |
| `poll(PollBudget)` | 在每次取出下一个回调前检查数量和时间预算 |
| `getReadyCount()` | 当前已入队回调数量的快照；不包含尚未到期的 timer 等待 |
| `run()` | 阻塞运行，直到 `stop()`；适合 worker |
| `runUntilIdle()` | 等待已启动的根任务完成并排空队列；适合程序收尾 |
| `stop()` / `restart()` | 停止取出新回调 / 恢复取出；stop 不取消任务 |

`PollBudget::m_max_callbacks` 和 `m_max_duration` 默认无限，任意一项为零均不执行回调。返回的 `PollReport` 包含实际回调数、耗时和触达的预算条件。触达预算不等于仍有积压，可以另查 ready count。

预算不能中断正在运行的回调，也不能排除操作系统抢占。耗时超过预算时，当前步骤仍会完成，然后停止取出下一个步骤。因此要把业务函数放到 worker，并保持 control 上的 completion、析构和其他直接 post 的代码足够小。

实现同时控制了库内的工作粒度：

- 进入节点和从前驱恢复时保留队列边界，长链的展开与失败短路分步推进。
- 调用任务前保留 worker 调度边界。
- 同一 junction 的等待者逐个通知。
- 同一源节点上的多个独立 merge 描述也逐个通知。

这样，限制出队数量才不会被一次大规模扇出通知绕开。任意用户协程内部的长循环或自定义立即完成 awaiter 仍需要自行协作；不要把 `co_await` 本身当成必然入队的操作。

`Signal(executor)` 的接收通知也逐个排队。无 executor 的 `Signal` 面向原始 sender/receiver 使用，允许在 `set()` 或请求取消的线程上直接完成 receiver，不提供控制线程预算边界。

## 实现阅读顺序

代码分成三层：`task` 负责依赖编排与结果策略，`core` 提供基础异步能力入口，`execution` 实现这些能力的执行机制。Asio 后端的对应执行机制来自第三方 Asio，因此两边的 `core`、`task` 可以处于相同的逻辑层次，底层代码量不需要相同。

```text
include/async/
├─ core/                        lcf::async 的类型别名和操作引入
├─ task/                        Chain、合流、结果和错误处理
└─ execution/                   lcf::async::execution 的原生能力
   ├─ coroutine/                Awaitable、yield、帧资源选择
   │  └─ details/               promise、awaiter、结果存储、帧缓存
   ├─ scheduling/               Context、Executor、QueueScheduler
   │  └─ details/               回调节点和就绪队列
   ├─ timer/                    delay sender
   │  └─ details/               timer 服务与等待注册
   ├─ synchronization/          Signal
   │  └─ details/               通知状态与等待注册
   └─ cancellation/             stop token 绑定
      └─ details/               取消注册竞争的 gate
```

`<async/core.h>` 提供与 Asio 相同职责的基础入口；`<async/execution.h>` 可独立使用，不包含 `core` 或 `task`。例如 `lcf::async::Awaitable<T>` 是 `lcf::async::execution::Awaitable<T>` 的别名，`Context`、`Executor` 和 `Signal` 同样直接选择实际类型。post、delay 和取消绑定使用薄适配，yield 直接引入原生操作，没有另一套运行时状态。具体的实际类型命名空间已改变，切换到本次结构后应重新编译消费方，不与旧二进制混用。

`FrameAllocatorScope`、`QueueScheduler`、`DelaySender`、`PollBudget`、`PollReport` 与 `bind_stop_token` 是后端专属能力，只在 `lcf::async::execution` 中提供。core 的 Context 仍是原生类型的别名，因此保留其预算方法；移除重复别名并不擦除实际类型的成员。

sender/receiver 是执行协议，协程是控制流表达方式；`execution/coroutine` 负责二者的适配，而调度、定时和通知也能直接服务非协程 sender。是否使用 sender 不决定代码所属层次：`InvocationSender` 涉及 `task::Result` 和任务调用策略，`Delivery` 涉及链的最终结果与 completion，二者仍留在 `task`。后端内部的任务适配可以连接执行层机制，对外不要求用户了解这些连接代码。

各组件的 `details` 只存放内部机制，不作为独立公开原语。模块导出实际执行类型及 `core` 的别名，promise、队列和帧分配辅助名称保持不可直接查找；头文件模式仍需携带模板依赖的定义。

| 文件 | 责任 |
| --- | --- |
| [core.h](include/async/core.h) / [execution.h](include/async/execution.h) | 基础能力入口与原生执行能力入口 |
| [Context.h](include/async/execution/scheduling/Context.h) / [Executor.h](include/async/execution/scheduling/Executor.h) / [QueueScheduler.h](include/async/execution/scheduling/QueueScheduler.h) | scheduler、普通回调 executor 与预算化出队 |
| [Awaitable.h](include/async/execution/coroutine/Awaitable.h) / [awaitable.h](include/async/execution/coroutine/details/awaitable.h) | 返回对象、自有 promise、帧所有权、对称转移、父上下文借用及 scheduler 修改时复制 |
| [FrameAllocatorScope.h](include/async/execution/coroutine/FrameAllocatorScope.h) / [frame.h](include/async/execution/coroutine/details/frame.h) | 帧资源选择、分配/释放配对、有限线程缓存与淘汰 |
| [common.h](include/async/task/common.h) | 结果归一化、调用约束、任务执行及 scheduler 切换 |
| [chain_details.h](include/async/task/chain_details.h) | 执行状态、节点 entry/完成连接、失败发布及稳定 operation 存储 |
| [Chain.h](include/async/task/Chain.h) | 前驱关系、短路、控制 executor 与执行状态 |
| [notification.h](include/async/execution/synchronization/details/notification.h) | 等待注册、单次完成、取消竞争、逐个通知 |
| [ChainJunction.h](include/async/task/ChainJunction.h) | 源结果观察、合流和分支构造 |
| [execution.h](include/async/task/execution.h) | 根 operation 的稳定存储和普通 completion 投递 |
| [operations.h](include/async/execution/timer/operations.h) / [timer.h](include/async/execution/timer/details/timer.h) | 原生 timer sender、取消注册、独立 timer 线程 |

scheduler 的 receiver 终结函数遵循 `noexcept` 要求。最终用户 completion 经预先分配的普通回调投递，因此它抛出的异常可以从 `poll` / `run` 调用处传播；不会直接穿过 receiver 的 `noexcept` 边界。

timer 使用一个进程内服务线程和按截止时间排序的链表，正延迟等待不会通过占用 worker 进行 sleep 模拟。当前插入和取消为 O(N)，尚未替换成堆或第三方容器。关闭运行时前，应完成或取消活动任务并继续排空其完成通知；`stop()` 只停止调度。

## 调度与分配成本

当前优化针对基准中暴露出的任务包装成本，参考了 Asio `detail/executor_op.hpp` 把 operation 本身作为队列节点、在终结回调前整理所有权的组织方式：

- 内置 `QueueScheduler` 经 `Executor` 调度时，直接把连接好的 operation 入队，省去再次经过 `any_scheduler` / `any_sender` 的路径。自定义 scheduler 仍使用原有擦除连接，保留它的 error 和 stopped 完成方式；没有假定所有 callback scheduler 都与内置队列等价。
- 同步函数由 `InvocationSender` 调用，不再为它额外创建 `Awaitable` 协程。sender 仍先排队、再检查取消、最后调用函数，因此没有把多个业务节点合并进一次 poll。
- `InvocationSender` 仅擦除调用入口，借用函数和参数；它们由正在等待的节点或重试协程持有，直到调用完成。三个调用点都在同一个 await 表达式中消费 sender，不允许传入临时函数或临时参数。这样既避免额外的拥有型函数包装，也阻止每个用户 lambda 的类型传播进整段 sender 组合。返回 awaitable 的任务仍使用协程包装。
- stopped 转换为 `operation_canceled` 时才构造异常对象，成功路径不再提前构造一份取消异常。
- 根 operation 的 holder 内嵌在 `Delivery`，省去一次独立分配；最终 completion 仍通过普通队列回调交付，异常传播方式保持一致。

后续进一步收紧了调度与控制层：

- `Awaitable<T>` 使用本库的返回对象和 promise。普通父子协程直接借用父 promise 的稳定 context，避免每次复制擦除后的 scheduler；父协程等待期间保证这份 context 的寿命。子任务请求切换 scheduler 时，先复制自己的 context，再修改 scheduler，最终通过退出清理回到原 scheduler。外部 promise 的环境可能是临时值，因此经 stdexec 接入时仍构造自己的 context。
- `YieldAwaitable` 只选择当前 promise 的 start scheduler，直接连接一次 schedule。它没有接受任意目标 executor 的入口；普通 `Executor::schedule()` 仍不被全局标成 affine。
- `invoke_task` 负责实际调用前的唯一 worker 投递；`spawn_result` 绑定目标 scheduler，不再先做另一遍 `starts_on`。异步调用的外层 sender 保持未知完成亲和性，让控制协程在任务完成后恢复到自己的 scheduler。
- 节点 entry 改用 `ExecutionSender`，去掉每个节点单独的 `execute()` 控制协程。它仅由使用同一 control executor 的链控制任务等待，entry 仍逐个调度；任务 operation、入口 operation 和函数捕获都保持在稳定存储中。调度连接异常、error、stopped 均先在 control 上发布节点失败，已注册的合流观察者仍会收到通知。函数捕获在调用结束后释放，不由结果 token 延长其生命周期。

普通子协程返回可以在一次 poll 中连续恢复；节点展开、从前驱恢复、通知分发和显式 yield 仍保留队列边界。预算约束的是这些协作点，不能保证任意用户协程中的循环每轮都被拆开。

内置队列仍支持多个 producer / consumer，没有增加队列依赖。帧分配和上下文管理由下述实现控制。具体收益、内存代价和测量范围见 [共享测试说明](../tests/README.md#第三轮自有-awaitable)。

## Awaitable 与帧分配

调用协程函数时创建返回对象和实际需要的帧；`initial_suspend` 让函数体保持惰性。开始等待时，awaiter 绑定 scheduler、取消 context 和 continuation。`co_return` 保存结果，`final_suspend` 对称转移到 continuation；调用者的 `await_resume` 移出结果或重新抛出异常，然后销毁子帧。stopped 使用 `with_awaitable_senders` 的停止传播协议，未恢复的帧由拥有它的 awaiter 清理。

这里拥有的是 `Awaitable` 的 promise 和生命周期，仍复用 stdexec 的 sender 等待适配、`exec::default_task_context` / `awaiter_context_t` 和退出清理等扩展。原生 `exec::task` 可以与它双向等待，`Awaitable` 也可以直接交给 `sync_wait`；没有修改第三方源码。

promise 的 `operator new` / sized `operator delete` 接到本库的帧分配实现。默认策略按 64 字节步长缓存小帧，每线程最多保留 64 KiB 空闲块，另有约 1 KiB 桶元数据；大于 8 KiB 或需要过对齐的块直接分配和释放。缓存满时优先淘汰较大的空闲块，避免旧负载占满缓存后，小帧一直无法进入。这个上限约束空闲缓存，不约束正在运行的协程总内存。

帧可以在创建线程退出后继续使用；默认块释放到当前线程的缓存。线程退出时释放缓存，若其他线程局部对象随后才销毁帧，则直接释放该块。缓存只保留原始存储，协程对象和参数已经析构。

需要自定义分配时，在普通同步作用域中使用 `FrameAllocatorScope`：

```cpp
std::pmr::synchronized_pool_resource frame_resource;
auto make_value = []() -> lcf::async::Awaitable<int> { co_return 42; };
auto operation = [&] {
    const lcf::async::execution::FrameAllocatorScope scope {frame_resource};
    return make_value();
}();
auto result = lcf::async::stde::sync_wait(std::move(operation));
```

作用域只选择当前线程在这段同步执行期间创建的帧，退出时恢复先前资源；它不会自动传给之后才创建的子任务。也可以在普通函数中包围 `start`、`poll` 或 `run`，覆盖其中发生的帧创建；scope 对象本身不能跨过所属协程的 `co_await`。资源必须活到所有相关帧释放，并支持实际释放线程。帧头记录原资源，销毁时不会改用当时的线程资源。

该钩子覆盖本库 `Awaitable` 实际发生的帧分配；编译器仍可省略分配。原生 `exec::task`、退出清理扩展、sender operation 和标准库对象各自管理其分配。状态对象的 PMR 与这个帧资源入口分别选择。

## 取消和错误

公共入口为 `lcf::async::CancellationSource`、`bind_cancellation(source, completion)` 和 `request_cancellation(source)`；本后端分别适配 std::stop_source、stop token 绑定和 request_stop。原生入口 `execution::bind_stop_token(std::stop_token, completion)` 可直接接受已有 token。取消结果使用 `std::errc::operation_canceled`，不会保留 Asio 的错误 category 身份。Asio 的信号/slot 与 std::stop_source 不具有相同的预请求或多订阅语义，共同名称不改变这些原生约束。

`on_error` 处理任务正常返回的失败结果，支持恢复、传播和在当前节点重试。异常和 sender 的 stopped 通道转为最终失败，不递归调用该错误处理器。被取消节点仍发布失败，包含其结果的 junction 得到通知；尚未执行的后继沿依赖关系短路。

链构造时选择的 `std::pmr::memory_resource` 管理任务状态和 junction 注册；`FrameAllocatorScope` 选择本库协程帧的资源。两者不表示标准库、原生 stdexec 扩展或系统分配都走同一个 resource。

通知和 timer 在交付最终 receiver 之前注销取消 callback，因此 receiver 可以立即销毁自己的 stop source。预取消 token 可能在 callback 构造期间同步调用取消函数；`StopRegistrationGate` 把这次完成延后到注册完成，再注销和交付，避免在构造期间销毁 callback。

## 编译与 VS Code

在 async 仓库根目录选择 preset：

| preset | 用途 |
| --- | --- |
| `stdexec` | 头文件模式，按目标延迟配置开发依赖 |
| `stdexec-ide` | 主构建中配置测试，可直接发现、运行和调试具体测试 |
| `stdexec-msvc-2026` | MSVC 2026 模块模式，主构建测试 |
| `stdexec-benchmark` | 只启用 stdexec benchmark，测试框架不因 benchmark 被引入 |
| `compare` | Asio/stdexec 共享比较基准，分别生成独立程序 |

```powershell
cmake --preset stdexec
cmake --build build/stdexec --config Debug --target async_stdexec_sandbox
cmake --build build/stdexec --config Debug --target async_stdexec_verify
cmake --build build/stdexec --config Debug --target async_stdexec_sandbox

cmake --preset stdexec-benchmark
cmake --build --preset stdexec-benchmark-release --target async_stdexec_benchmark

cmake --preset compare
cmake --build --preset compare-release
```

Ninja + MSVC 终端需要相应的编译器环境；VS Code CMake Tools 使用对应 kit/toolchain。模块模式下 `async_stdexec` 是实际编译的库；头文件模式下它是 INTERFACE target。`async_stdexec_sandbox` 同时验证公开入口的编译和示例运行。

源码消费方链接 `lcf::async_stdexec`。整体选项由 `async/cmake/options.cmake` 声明；stdexec 局部开关见 [options.cmake](cmake/options.cmake)。`LCF_ASYNC_BUILD_TESTS=OFF` 关闭全部开发内容。默认依赖来源 `PACKAGE`，开发 preset 明确选择 `FETCH`；`PROVIDED` 要求父构建先提供 target，不能把 target 跨进程传给延迟子构建，需选 tests in main build。

`deps.json` 固定 stdexec revision，`fetch_deps.cmake` 读取依赖清单并适配 target；支持 CMake 的 `FETCHCONTENT_SOURCE_DIR_STDEXEC` 本地覆盖。安装包消费只查找 stdexec 和 Threads，不获取测试框架或 Asio。

## 模块与标准库迁移

公开入口始终为 `<async/task.h>`。模块模式先在全局模块片段包含第三方依赖，再在 `export module lcf.async;` 后包含同一份实现头文件，由独立一行的 `LCF_ASYNC_EXPORT` 控制导出。

工程应统一选择头文件或模块模式。`LCF_ASYNC_FORCE_HEADERS` 和 IntelliSense 的头文件分支用于明确覆盖和编辑器解析，不承诺跨模式传递对象。具体契约见 [modules 设计](../docs/cpp-modules-transition.md)。两个后端的公开命名空间和模块名相同，因此比较基准使用独立可执行文件，不在同一程序中链接两个实现。

[stdexec.h](include/async/execution/stdexec.h) 集中定义 `namespace stde = stdexec`。标准接口未来对应 `std::execution`；当前仍使用 `exec::default_task_context`、`awaiter_context_t`、`at_coroutine_exit`、完成亲和性查询和 `any_scheduler` 等扩展，替换别名不能完成全部迁移。

此前 Clang 22.1.1 / MS ABI 的“原生 task 返回对象再转换成门面”路径会递归析构。自有 promise 直接返回 `Awaitable` 后，已移除整类 task 回退到原生类型的分支。

这组 Clang / MSVC 标准库组合还存在过对齐 promise 的地址计算问题：标准库向 Clang 的 coroutine promise builtin 传入零对齐，而实际 promise 可能需要更高对齐。当前仅在该组合下，把超过默认 new 对齐的返回值放到独立存储，保持 promise 的常规对齐；帧分配仍满足返回值所需对齐。普通返回值继续内联存储。该场景已由 `alignas(64)` 返回值和 ASan 回归验证。

## 验证记录

2026-10-19 本机验证：

- MSVC 19.44 头文件 Debug 137 项、Release 133 项，MSVC 2026 模块 Debug 139 项及各自 sandbox 通过。Release 少四项断言 death test；模块多两项跨翻译单元与私有名称检查。
- 新增十项 Awaitable 生命周期/分配/互操作测试、四种通知/timer 终结时立即销毁 stop source 的场景。子任务跨 scheduler 返回和异常测试还检查父任务后续 yield 的线程，验证上下文没有被子任务修改。
- 最终版本的 30 个帧、原生调度、合流和取消场景在模块模式下重复 100 次，Clang ASan 下重复 20 次；其中通知/timer 竞争与合流场景各自含 64 轮。
- Clang 22.1.1 / MS ABI、`-O1`、AddressSanitizer 下的 137 项完整接口和 sandbox 通过。该配置使用动态 Release CRT，保留断言。GCC 16.1.0 头文件编译及 0～1024 个立即子任务负载运行通过；没有执行 GCC 完整契约套件。
- Release 共享比较全部 10 类负载通过；与 `78c851a` 的固定程序交错测量各 3 轮。Asio 对照程序 SHA256 相同；报告回归测试通过。原始数据与程序在忽略的 `build/optimization3/`。
- 此前延迟 example/verify/sandbox、独立 benchmark 和共享比较目标通过；sandbox 已验证头文件、模块和延迟构建入口。仅 benchmark 的子构建不引入 GoogleTest。
- 此前 `PROVIDED`、`PACKAGE`、缺包不下载、头文件安装与移动安装前缀后的独立消费已验证。本轮未重跑安装消费矩阵；模块安装消费未做完整矩阵验证。

头文件开发目标关闭 CMake 模块扫描，避免无 import 的 GCC 翻译单元也被加上模块编译参数；本机 GCC 在该路径曾发生编译器内部错误。消费方若自行启用全局扫描，需要结合自己的工具链配置。

Asio 头文件公共测试通过；额外执行 Asio 原有模块入口的完整 verify 时，本机 MSVC 2026 遇到 C1116，诊断涉及 `asio::config_service` 的 IFC 导入。stdexec 模块不依赖该入口。这项 Asio 模块验证尚未通过。
