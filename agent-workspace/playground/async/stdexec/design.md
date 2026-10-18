# stdexec 后端设计

本文定义 stdexec 后端的目标接口、行为契约和实现边界。具体落地顺序见 [implementation-guide.md](implementation-guide.md)。本目录当前用于存放设计，不代表后端已经实现或接入构建。

## 1. 目标与兼容范围

业务代码始终使用 `lcf::async::task`。Asio、stdexec 和未来的标准库后端提供相同的公开接口和行为契约；`details::` 中的类型、算法、同步方式和文件划分可以不同。

兼容以经过 common 别名统一后的接口为基准：

| 范围 | 要求 |
| --- | --- |
| 公开 API | 名称、重载形状、模板结果类型、const 和引用限定、可复制/移动性质、公开异常规格一致 |
| 任务返回类型 | 使用 `task::Awaitable<T>`；其底层协程类型随后端选择 |
| 调度参数 | 使用 `task::Executor`；其底层调度句柄随后端选择，`getExecutor()` 返回同一公开别名 |
| 运行时行为 | 结果归一化、执行线程、错误处理、取消边界、生命周期和资源传播一致 |
| 使用入口 | 保持 `<async/task.h>`、`LCF_ASYNC_USE_MODULES` 和 `lcf.async` 的使用方式 |
| 内部实现 | 不要求复用 Asio 的 coroutine、channel、operation 包装或 `details::` 名称 |

这是源码接口与行为兼容约定。不同后端的具体类型和布局可以不同，不承诺 ABI 相同。一个程序及其公共接口相连的组件必须选择同一后端；同一个构建工程可以生成使用不同后端的独立测试程序。

运行环境的创建属于集成层：Asio 的 `io_context`、原生 sender 的 scheduler、主线程泵和 timer 不要求有相同的后端 API。它们进入 `task::` 时统一为公开别名。

## 2. common 中的后端别名

在各后端的 common 中提供相同名称。Asio 侧的目标形式为：

```cpp
namespace lcf::async::task {

using Executor = asio::any_io_executor;

template <typename T = void>
using Awaitable = asio::awaitable<T>;

template <typename T>
using Result = std::expected<T, std::error_code>;

template <typename T>
using ResultHandle = std::shared_ptr<const Result<T>>;

}
```

节点和错误处理器分别使用 `Awaitable<T>`、`Awaitable<Result<T>>`、`Awaitable<ErrorAction<T>>` 等已有返回形状。stdexec 侧优先验证 `exec::task<T>` 及其所需环境配置能否作为 `Awaitable<T>` 的别名目标，相关差异封装在 `details::`；未来再验证标准库 task 的适配。

别名统一协程返回类型。协程体内的 `co_await asio::post(...)`、`this_coro::executor`、timer 和 channel 操作仍属于 Asio 集成。跨后端的业务协程应使用运行环境适配操作；保留既有 Asio 协程体时，通过专门桥接运行它。不能仅替换返回类型后假设任意 Asio awaiter 都能被原生 task 接受。

迁移现有 Asio 实现时，应同步替换公开声明、类型推导、约束和模块导出中的对应类型。仅新增别名但仍在公开声明中散落底层类型，不能作为完成条件。

别名整理完成后，下面的任务构造代码在各后端保持相同：

```cpp
namespace task = lcf::async::task;

task::Chain<int> make_chain(task::Executor worker)
{
    return task::Chain {41, std::move(worker)}
        .link([](const int & value) -> task::Awaitable<task::Result<int>> {
            co_return task::Result<int> {value + 1};
        });
}
```

运行环境如何创建 worker 不影响此函数的公开签名。

## 3. 公开接口清单

以下名称均属于 `lcf::async::task`；受约束模板的具体约束仍以现有公开声明及编译期测试为基准。

| 类型或函数 | 必须保留的接口性质 |
| --- | --- |
| `Result<T>`、`ResultHandle<T>` | 分别为 expected 和不可变结果的共享句柄 |
| `make_result_handle(owner_sp, result_p)` | aliasing ownership；保留原始结果地址；构造为 `noexcept` |
| `Chain<T>` | 外部类型只取决于结果 T；move-only；保留 `value_type` 和嵌套 `ResultToken` |
| Chain 构造 | 值种子或 void 种子、Executor、可选 memory_resource；保留相应 CTAD |
| `Chain::link()` | 仅在右值调用；支持继承或显式 Executor、可选 ErrorHandler、额外 tokens；返回 `Chain<U>` |
| `Chain::start()` | 仅在右值调用；显式 completion Executor 和 completion；返回 void |
| Chain 查询 | `getResultToken()`、`getExecutor()`、`getMemoryResource()` |
| `Chain<T>::ResultToken` | 可复制的 opaque token；不提供公开结果读取或 ready 查询 |
| `ChainJunction<Ts...>` | 可复制 descriptor；由 merge 创建；保留 `getMemoryResource()` |
| `ChainJunction::link()` | const 调用；要求显式 Executor；支持 handler 和额外 tokens；返回 `Chain<U>` |
| `merge(tokens...)` | 至少一个真实 ResultToken；另有首参数为 memory_resource 的重载 |
| `ErrorHandleContext` | 保留构造及 `getErrorCode()`、`getAttemptCount()` |
| `RetryAfter`、`ErrorAction<T>`、`ErrorHandler<H>` | 保留现有公开表示与成员，包括 `m_delay`、`m_function` |
| `on_error()`、`propagate()`、`retry_after()`、`use_value()` | 保留返回形状、约束及条件 `noexcept`；use_value 包括 void 重载 |

不为原生 sender 额外开放不同的 Chain 模板参数，也不在某个后端单独添加 token 读取、自动启动、无 Executor 的 start、junction.start 或 Chain.run 等接口。

## 4. 共同的行为契约

契约编号用于实现指导和跨后端测试之间的映射。

### C01：所有权与启动

Chain 构建和 link 不执行用户任务，种子结果在构造后已经 ready。link 和 start 消耗来源 Chain；已消耗或 moved-from Chain 不能再次执行。start 后执行状态由运行中的操作持有，临时 Chain 销毁不影响执行。

### C02：执行位置

每个节点在自身指定的 Executor 执行，未显式指定时继承前驱 Executor。节点中的异步任务、错误处理器及重试遵循同一执行位置。completion 在 start 指定的 Executor 执行，包括失败和已经 ready 的情况。

保留现有用户节点进入调度队列的边界，不能直接用 `just | then` 在 start 调用栈执行用户节点。额外 token 不负责启动任务或选择执行位置。

### C03：返回值归一化

| 节点返回或 Awaitable 的最终返回 | 节点结果 |
| --- | --- |
| 普通 T | `Result<T>` 成功 |
| void | `Result<void>` 成功 |
| 零值 `std::error_code` | void 成功 |
| 非零 `std::error_code` | void 失败 |
| `Result<T>` | 保留原有 success/failure 状态和内容 |
| `Result<std::error_code>` 中的非零成功值 | error_code 作为成功载荷保留 |
| `std::unexpected(std::error_code {})` 所构成的 Result | 仍为失败 |

同步和异步返回执行同一套规则。不能依赖 error_code 的 bool 值代替 expected 的状态。单独返回 unexpected、嵌套 awaitable 以及其他未被现有约束接受的形状，不因引入 sender 而自动成为新增公开能力。

### C04：参数展开与结果身份

非 void 结果以 `const T &` 传入，void 结果不增加参数。顺序为直接前驱或 merge 输入顺序，随后是额外 token 顺序。普通 tuple 是一个参数，不自动拆成多个参数。重复 token 指向同一个结果对象，move-only 值不复制、不被某个消费者取走。

### C05：失败与异常

前驱失败时跳过后继任务及其本地错误处理器。节点或其结果转换、发布中的异常统一发布为失败：system_error 保留 code，其他异常为 io_error。包括 system_error 携带零码的情况。

必须区分“节点正常返回失败”和“节点抛异常”。现有 on_error 只处理前者；节点、handler 或结果移动抛异常时，不能通过同一个 handler 再处理一次。构建、分配、启动接入等同步阶段的异常仍遵循公开函数的异常规格，不能承诺所有异常都转换成节点结果。

### C06：本地恢复与重试

attempt 从 1 开始；只重试当前节点，不重跑上游。多次尝试保留同一个 function、handler 及输入对象身份，支持 move-only 状态。最终恢复值成为该节点唯一的公开结果。

正 retry delay 使用异步计时，不阻塞工作线程；零或负 delay 不启动 timer。每次尝试仍遵循节点调度边界。成功后不调用 handler；propagate 保留明确的失败状态，包括零码；handler 抛异常不递归进入 handler。取消 retry timer 后不再额外调用 handler。

### C07：merge 的独立启动与登记时机

merge 只描述和登记依赖，不消费或启动来源 Chain。junction successor 可以先启动等待，来源由调用方分别启动，继续使用各自 Executor。

对未完成来源，merge 必须在来源 operation 被取走执行之前登记通知；对已经完成来源，可以从保留 token 构建新 junction。stdexec 即使支持运行中动态订阅，也须保留当前 Debug 非法使用约束。

### C08：merge 的完成与错误选择

所有输入完成后，才能执行后继或发布来源失败；一个来源提前失败不能让 junction 提前完成。多个失败按输入 token 顺序选第一个，不能按完成顺序选择，也不能丢掉零码失败。

重复 token、void 输入、中间节点 token、嵌套 junction 和同一 descriptor 的多个分支均受支持。来源任务只执行一次；多个分支分别调度，看到同一来源结果。

### C09：额外 token 的约束

Chain 和 ChainJunction 的额外 link tokens 在使用时必须已经完成；它们不被隐式等待或启动。独立未完成来源必须放进 merge 输入。

“已完成但失败的额外 token”尚缺少明确的共同测试：当前非 void 的 value 访问可能抛 bad_expected_access，而 void 展开不读 value。落地时先补足现状测试或明确统一修订两端契约，不把它未经说明地改成 merge 的失败传播规则。

### C10：分支取消与错误表示

取消某个 junction 分支只终止该分支的等待，不能取消来源或其他分支；取消结果也必须发布到该分支状态，使其 token 的观察者能够完成。

当前测试分别要求 retry timer 取消为 `asio::error::operation_aborted`，junction 等待取消为 `asio::experimental::error::channel_cancelled`。严格兼容阶段保留这些 error_code 的类别与值，集中映射；不能仅因都表示取消而替换成 `std::errc::operation_canceled`。

既有 completion 关联的 Asio cancellation_slot 由兼容适配器接入分支 stop source。原生运行环境的取消接入放在运行环境适配层。以后移除 Asio 错误或取消桥接时，须同步调整两端的共同契约。

### C11：completion 边界

正常异步完成路径只交付一次 completion，参数引用已发布的原始 Result。用户 completion 抛出的异常不再转换成任务失败，不二次调用 completion，而是到达运行 completion 的 context/pump 调用者。

原生 receiver 的完成操作必须 noexcept。它应将用户 completion 安排到运行环境的普通回调边界；不能直接在 set_value/set_error/set_stopped 中调用可能抛异常的用户代码。标准 run_loop.run 为 noexcept，不能直接用它复刻当前异常出口。

### C12：结果与执行捕获的生命周期

ResultToken 和 ResultHandle 保留原始发布状态与不可变结果，成功和失败都不通过复制另建结果。make_result_handle 使用 aliasing shared ownership。

长期保留结果不应长期保留已完成的 task/handler 捕获。成功、正常返回失败、同步异常、挂起后异常都必须释放执行捕获。operation_state 的地址稳定性和执行期间存活由后端保障，不能依赖调用者保留 Chain 或额外 scope。

### C13：memory_resource

Chain 的节点状态沿 link 继承资源；merge 默认使用当前默认资源，也可显式指定资源；junction 的后继及等待设施使用 junction 自己的资源，不借用来源资源。

保留现有已验证的节点/结果状态分配时机和最终平衡释放。契约不固定两个后端的总分配次数，也不声称底层 scheduler、所有 coroutine frame 或库内部容器都受该资源控制。不能把项目拥有的状态偷偷转到全局分配器来绕过资源测试。资源必须比使用它的状态、句柄和等待操作存活更久。

### C14：编译期约束与模块

保留现有公开正例和反例：参数类型匹配、handler 与节点结果匹配、真实 token 限制、空 merge 拒绝、rvalue link/start、descriptor 的 const link 等。

头文件和模块消费者在同一后端下能交换 Chain、junction 和 token。模块导出公开别名和接口，不导出 details；不同后端无需提供同名 details 或相同的内部 noexcept 辅助函数。

## 5. stdexec 实现分层

| 层 | 职责 |
| --- | --- |
| task 公开层 | 固定 Chain、Token、Junction、Result 和错误处理接口，以及 Awaitable/Executor 别名 |
| 节点与结果层 | 单次发布、不可变共享结果、执行所有权、输入引用、节点重试和来源/分支依赖 |
| details 执行适配层 | sender 构造、connect/start、返回类型识别、调度、stop 环境、timer、completion 交付 |
| 运行环境层 | 工作线程资源、主线程非阻塞泵、异步 timer/I/O、Asio 兼容桥接 |

stdexec 的 value/error/stopped 是底层完成通道。项目可以让“来源已完成”的通知 sender 在 value 通道传递 `ResultHandle<T>`，即使其中 Result 为失败；到项目节点边界再检查 Result。这样不会让原生组合算法把领域失败自动解释成取消兄弟操作的信号。

返回失败、抛异常和停止仍须保留来源信息，以满足 C05、C06 和 C10，不能提前全部压成一种 error_code 后再决定是否调用 handler。

### 原生算法的使用边界

| 算法或机制 | 使用方式 |
| --- | --- |
| then | 同步调用；结果仍经公共归一化，不能将 Awaitable 当普通载荷 |
| let_value | 拼接异步节点或实现分阶段操作 |
| schedule / starts_on / continues_on | 维护节点及 completion 的执行位置；显式验证错误和停止通道的位置 |
| upon_error / let_error | 内部错误转换；不能不加区分地用于公开 on_error |
| when_all | 可组合只等待通知的 senders；不直接接管或启动来源 Chain |
| stop token | 分支等待与来源执行分别持有取消域 |
| scope / operation holder | 内部托管 start 后的操作，不新增调用者必须持有的 scope 参数 |

Executor 适配器需要同时支持节点调度和 C11 的普通 completion 交付边界。仅将任意 scheduler 做类型擦除，并不自动满足完整 Executor 契约。

不依赖 split、ensure_started、原生算法的错误竞争结果或默认取消传播来定义 ResultToken 和 merge。

## 6. C++26 标准库接管

标准库接管执行适配层可用的标准设施，task 层的接口及 C01—C14 保持稳定。

| 设施 | 迁移要求 |
| --- | --- |
| sender/receiver、connect/start、通用组合算法 | 优先依赖已标准化的协议与算法，在 details 中隔离实现差异 |
| task、run_loop、counting scopes、parallel scheduler | 当前草案已包含；逐项验证目标标准库的实际支持和语义 |
| exec::task、exec::async_scope、类型擦除等 stdexec 扩展 | 不因出现在 stdexec 仓库就视为标准；按固定依赖版本逐项适配 |
| 自定义主线程 scheduler | 继续实现非阻塞 poll；标准 run_loop 没有 poll_one 接口 |
| 异步 timer/I/O | 保留项目或第三方运行环境适配 |
| 共享结果、确定性错误选择、重试策略、Asio 兼容 | 继续由项目实现 |

stdexec 到 std::execution 的切换条件是所用能力的编译探测和共同测试通过，不能只依据 C++26 编译选项或替换命名空间。

## 7. 基准源码与参考资料

| 基准 | 位置 |
| --- | --- |
| 公开别名、约束、归一化 | [common.h](../libs/async/asio/include/async/task/common.h) |
| Chain 与启动 | [Chain.h](../libs/async/asio/include/async/task/Chain.h) |
| token 与发布状态 | [chain_details.h](../libs/async/asio/include/async/task/chain_details.h) |
| junction | [ChainJunction.h](../libs/async/asio/include/async/task/ChainJunction.h) |
| 错误处理与重试 | [error_handling.h](../libs/async/asio/include/async/task/error_handling.h) |
| 行为及编译期基准 | [verify](../libs/async/asio/tests/verify) |
| 模块导出 | [async.cppm](../libs/async/asio/modules/async.cppm) |
| 测试写法 | [C++ Tests 风格](../../../conventions/cpp-style.md#tests) |

标准相关资料核对日期：2026-10-18。

- [NVIDIA/stdexec 官方说明](https://github.com/NVIDIA/stdexec)
- [执行库 synopsis](https://eel.is/c++draft/execution.syn)
- [when_all 的启动、错误及停止语义](https://eel.is/c++draft/exec.when.all)
- [run_loop 接口及 noexcept 边界](https://eel.is/c++draft/exec.run.loop)
- [parallel scheduler](https://eel.is/c++draft/exec.par.scheduler)
