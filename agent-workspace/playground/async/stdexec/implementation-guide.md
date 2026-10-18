# stdexec 后端实现指导

本文是 [design.md](design.md) 的落地顺序和验收说明。C01—C14 的含义以设计文档为准。本次只落地文档，后续实施再创建源码、测试及构建目录。

## 1. 先建立公共接口基线

1. 在 common 引入设计中的 `Awaitable<T>` 和 `Executor`，先让现有 Asio 侧通过别名工作。
2. 同步修改公开签名、CTAD、返回值识别、handler 约束、测试声明及模块导出。使用别名的节点应能继续返回普通值、void、error_code 和 Result。
3. 将跨后端测试中的运行环境操作封装起来，包括 worker/control Executor、非阻塞泵、调度让步、延迟、gate 和取消绑定。保留真正异步的挂起、线程切换和取消，不能把异步用例改写成同步执行来通过测试。
4. 将公开编译期契约与 Asio details 检查区分开。两个后端共享公开检查；内部辅助函数的类型和 noexcept 检查留给各自实现。
5. 形成固定的公开接口清单与共同场景集后，再落地 stdexec 侧。

当前 tests/common/Runtime.h 负责调度和原始结果保留，可作为适配入口。现有部分场景还直接创建 Asio context、timer、channel 和 cancellation_signal，因此只替换 Runtime.h 不足以复用全部测试。抽取运行环境操作后，场景和断言主体应共用。

保留旧 Asio 调用的兼容测试，避免公共别名整理时意外改变现有后端。

## 2. 固定依赖并验证最小执行能力

固定一个 stdexec tag 或 commit，不使用浮动 main 作为可复现基线。依赖获取及版本记录沿用实验项目已有方式；先验证当前 GCC、Clang、MSVC 所需组合，不把全部模块构建能力假设为已经可用。

原型阶段只验证以下机制，不先实现全部 Chain：

| 机制 | 原型验收 |
| --- | --- |
| Awaitable 别名目标 | 支持 co_return 普通值、void、Result、ErrorAction；挂起后恢复；异常和停止可识别 |
| Executor 适配 | 能持有不同实际 scheduler，并维持单一公开 Executor 类型 |
| 调度 | 从控制线程进入 worker，再返回指定 completion 线程；同 Executor 也遵循节点排队边界 |
| 原生 operation_state | connect 后地址稳定；不可移动的 state 可以就地构造并托管 |
| 结果通知 | 已完成和未完成两种路径都恰好一次唤醒 |
| 停止 | 等待取消不影响生产者；已请求停止和完成竞争均有终态 |
| completion | 用户回调异常到达普通 context/pump 调用者，未穿过 noexcept receiver |

`exec::task<T>` 是候选别名目标，其环境、scheduler 亲和性和停止行为须在该固定版本上验证。若需要特定环境参数，把配置和 traits 收口到 details，不向 task::Chain 增加后端模板参数。

原生 task 并非自动兼容 Asio awaiter。既有协程体通过 Asio 桥接，通用场景通过运行环境适配操作；分别验证两条路径。

## 3. 收口执行适配机制

以下是内部职责划分，不要求采用这些内部名字，也不新增 task 公开 API：

| 内部机制 | 输入/输出与职责 |
| --- | --- |
| 调度适配 | Executor 到受约束的调度 sender；保证支持的节点执行位置与排队边界 |
| 调用适配 | function、输入引用到正常返回/异常/停止的可区分完成信息 |
| 结果归一化 | 正常返回的值到 Result；与 Asio 共用规则 |
| 结果订阅 | 来源状态到通知 sender；完成时提供原始结果的共享身份，不启动来源 |
| 延迟适配 | duration、节点 Executor、取消域到异步 timer 操作 |
| 启动托管 | 固定地址的 operation_state、运行期间的所有权和结束回收 |
| 取消适配 | 关联 cancellation_slot 或原生停止环境到该分支自己的 stop source |
| completion 交付 | 在指定 Executor 的普通回调边界调用用户代码 |

Executor 若需要类型擦除，由项目适配器保证契约，不把某个库的 any_scheduler 当作 C++26 必备设施。Awaitable traits 也从后端适配中获取值类型，不假定所有 task 都有 Asio 的 value_type 成员。

不把任意 sender 新增为自动启动或展开的异步返回形状。符合原有普通值约束的 sender 对象仍可作为普通载荷，不能仅因它满足 sender concept 就改变其含义。先保持 C03 和 C14 的返回集合。

## 4. 结果状态与运行状态

结果状态至少负责结果存储、发布标记、资源来源和通知登记。执行生命周期另行负责 callable、参数引用、operation_state、取消回调及托管引用。

### 单次发布

1. 在最终存储位置构造 Result。
2. 构造成功后，以 release 语义发布 ready；消费者以 acquire 观察。
3. 正常成功值移动或构造抛异常时，在同一状态中发布转换后的失败。
4. 失败通路不再尝试移动成功值，也不再次执行用户函数。
5. 发布完成后才通知等待者，且每个登记项只通知一次。

必须有区别于 error_code bool 值的失败表示。使用 expected 状态或 `optional<error_code>`，保留零码失败。

既有“每个结果移动位置注入异常”的测试，应按各后端成功路径测得的移动次数分别注入；不要求两个后端移动次数相同。

### 资源与回收

结果与执行捕获可以放在不同对象中，也可以分区管理，但 token/ResultHandle 对结果的所有权不能把整个已完成运行图永久保留下来。

connect 产生的 operation_state 可能不可移动。通过工厂在稳定存储中直接构造，不先返回到临时变量再移动进容器。可以在构建阶段预留节点运行存储，满足现有执行期间不新建节点/结果状态的资源检查。

完成后的清理顺序必须满足：不再使用输入引用和 callable，释放执行捕获，保留结果及必要所有者，再交付外部观察。回收 operation_state 时遵循 sender/receiver 的生命周期规则，避免在其成员仍被调用时随意销毁存储。只应在明确允许的终端位置销毁，或通过不会延长用户捕获的独立托管机制回收。

来源发布、分支取消和最终完成都要解除取消注册及订阅，避免 token 长期保留已结束分支。不得形成“运行 holder 持有自己且完成后未断开”的循环。

## 5. 实现 Chain 的最小闭环

先实现无 handler、无额外 token 的同步节点：

1. 构造 seed state 并立即发布种子。
2. link 消耗前驱句柄，创建只由结果 T 决定的公开 Chain<U>。具体 sender 类型在 details 擦除。
3. 节点执行先完成直接前驱；如果失败，原样发布失败并跳过 function。
4. 将该节点安排到自己的 Executor，再以 const 引用调用 function。
5. 对正常返回执行公共归一化；对异常执行异常转换；发布结果并完成节点操作。
6. start 转移最终运行图的所有权，由内部 holder 托管；调用者不需要另存 sender、operation_state 或 scope。
7. 结果回到指定 completion Executor，恰好交付一次。

然后增加 void、move-only 值/函数、普通 tuple 和额外 tokens。额外 token 只检查就绪并构造引用参数，不使用原生 when_all 来隐式等待它们。

异步节点用 let_value 或等价手写组合展开。不能用 then 返回 Awaitable 后直接将其作为成功载荷。保留 C05 的异常来源信息，避免下一步错误处理误接收异常。

## 6. 实现 completion 的异常出口

内部 receiver 的 set_value/set_error/set_stopped 均不向外抛异常。它们整理结果及所有权，并安排一个普通 completion 回调。

优先在 start 的同步阶段准备交付记录和必要的队列存储，使终端 receiver 的入队不再分配、不抛异常。若使用可能分配的 post 操作，必须提供相应分配策略及失败处理，不能假定 post 本身满足 noexcept。

- Asio 兼容运行环境可向目标 Executor post 普通 handler，由 io_context 的调用边界传播用户 completion 异常。
- 原生运行环境提供自己的普通回调队列和可抛异常的 pump 边界。调度 sender 的 receiver 完成仍遵守 noexcept，普通用户 completion 在独立边界调用。
- 如果使用标准 run_loop 作为内部资源，不能让用户异常直接穿过 run_loop.run；需要上述普通回调出口。

不能用 catch-all 把 completion 异常转换成第二次任务完成，也不能在 catch 中再次调用 completion。交付期间由句柄保留原始 Result，失败结果同样不复制。

调度接入失败、资源耗尽等情况应按其发生阶段明确处理：start 的同步异常遵循已有非 noexcept 接口；异步阶段不能让结果永远不发布、订阅永远不完成。完成回调自身的异常保持 C11。

## 7. 增加错误处理与重试

公开 on_error 不直接等同于 let_error。推荐以原生 task 或内部状态机实现节点本地循环：

1. 持有同一个 function、handler 和输入引用，attempt 从 1 开始。
2. 调用当前节点；正常成功直接返回最终 Result。
3. 仅正常返回失败时，用 ErrorHandleContext 调用 handler。
4. Result 动作直接成为最终结果；propagate 的零码仍表示失败。
5. RetryAfter 为正时异步等待 timer；否则进入下次尝试，保留正常节点调度边界。
6. 节点或 handler 抛异常时终止本地循环，走异常发布；不再调用 handler。
7. timer 被取消时发布兼容取消错误，不再次调用 handler，也不向外公开中间尝试的失败。

不能每次尝试重新移动构造 function/handler，不能重建输入值，不能重新启动整个前驱 sender。保留输入地址、callable 状态、attempt、handler 次数和上游执行次数的独立断言。

## 8. 实现结果订阅与 ChainJunction

### 登记与单次通知

merge 保留输入 token，并在尚未执行的来源上登记完成通知；登记不启动生产者。每个输入位置都需要有明确的到达规则：

- 重复 token 可按每个位置分别登记，也可对来源去重后记录所有输入位置，但计数和参数顺序必须一致。
- 构造时已 ready 的输入直接计为完成；未 ready 的输入只在来源最终发布后到达。
- 订阅与发布之间使用同一锁下的二次 ready 检查或等价原子协议，覆盖“先检查未 ready，登记前刚好完成”的竞争。
- 不在来源锁内执行用户代码或调用可能重入的 receiver。
- 保留 C07 的登记时机检查，不能因为新的登记机制更灵活而删除非法使用契约。

### 等待与选择

junction successor 等待全部来源位置完成后，按原始 token 顺序扫描 Result，选出第一个失败；否则构造输入引用，执行自己的节点。

原生 when_all 只能等待“订阅来源完成”的 sender，不接管生产者。领域失败作为 Result 数据保留，不经 when_all 的原生错误竞争来选择。若使用自定义计数和通知，实现同一行为即可。

descriptor 的多次 link 创建独立分支等待。每个分支携带自己的 Executor、取消域、结果状态和额外 tokens，来源结果可共享。

### 取消竞争

分支等待的停止请求只注销或结束自己的等待，不传给来源的执行 stop source。完成与取消竞争必须通过单一终态决议，保证恰好交付一次，且取消回调结束前不回收它仍在访问的 waiter。

停止后仍应发布该分支的失败状态并通知其观察者。另一分支继续等待，来源只执行一次。清理一个 waiter 不能关闭所有分支共享的通知设施。

集中适配现有 Asio cancellation_slot 和取消 error_code。不得克隆 error_category 的名称或数值来假冒同一 error_code；过渡期可以引用对应 Asio 错误定义来保证类别身份。未来去除该依赖需要两端共同迁移。

## 9. 运行环境与资源

工作线程资源必须提供任务所需的并行执行能力；渲染/控制线程运行环境必须支持每帧非阻塞处理任务。不能用一个持续阻塞的 run_loop.run 替代当前主线程 poll_one 交互。

正延迟重试需要可取消的异步 timer。可以先桥接现有 Asio timer，后续替换 timer 实现而不改 task 接口。不能用 sleep_for 阻塞 worker 模拟异步延迟。

严格按 C13 分配项目拥有的节点状态、junction 状态和等待设施。资源测试区分来源与 junction 的计数，验证继承、选择和最终释放，不固定跨后端内部结构的分配次数。

底层库不支持 allocator 注入的地方应如实记录。已有“不新增节点/结果状态分配”的检查不应扩大成“整个异步运行零分配”，也不能通过把状态改成全局分配来规避。

运行环境和 memory_resource 都必须覆盖运行中操作的生命周期；保留结果可以超过 runtime 生命周期，但只要结果仍使用该资源，该资源仍须有效。

## 10. 契约验证与失败定位

保持共同场景源和公开编译期断言，针对不同后端提供运行环境适配。每个 executable 只链接一种后端。

| 契约 | 复用的验证入口 | 失败时优先检查 |
| --- | --- | --- |
| C01、C02 | chain_test 的执行路由、临时 Chain、单次启动；contracts 的非法使用 | 状态转移、operation holder、调度适配、completion Executor |
| C03、C04 | ChainReturns、token 顺序、tuple、move-only 场景 | 返回 traits、归一化、引用 tuple 组装 |
| C05 | ChainTaskExceptions、结果移动注入、失败跳过后继 | 异常来源、发布顺序、失败状态分支 |
| C06 | ErrorRetries、RecoveryPayloads、UpstreamFailures、ErrorPropagation、ErrorExceptions | 本地循环、attempt、handler 调用条件、timer |
| C07、C08 | chain_junction_test 的独立启动、完成顺序、共享完成、多分支 | 登记时机、计数、通知、按输入顺序的错误扫描 |
| C09 | contracts 的 unfinished additional token 检查 | link 参数路径；不能误接入异步等待 |
| C10 | retry 取消、junction 分支取消和该分支 token 的观察者 | 关联取消适配、停止域、waiter 终态、错误类别 |
| C11 | completion 执行线程、次数、异常传播 | 普通回调交付边界、结果保留、noexcept receiver |
| C12 | ChainCaptureLifetime、ResultStateLifetime、来源销毁后的 move-only token | 执行捕获释放、aliasing ownership、运行 holder 回收 |
| C13 | 来源/merge 分离 tracking resource 场景 | 节点资源继承、等待设施分配、最终引用 |
| C14 | contracts 公开静态断言、header_bridge、module privacy probe | 公开重载与约束、导出清单、同一后端的类型一致性 |

用命名参数覆盖返回形状、异常来源、重试边界、来源成功/失败组合及两种完成顺序。一次失败对应一个可观察契约，不把所有边界塞进一个无法区分的集成测试。

并发场景用 gate/signal 控制顺序，用 deadline 防止挂死；不靠 sleep 猜测谁先完成。保留阶段、分支、迭代和注入位置的诊断。callback 中先记录完成 bookkeeping，再执行 fatal assertion，避免断言被后续超时掩盖。

每个场景继续提供 Objective、Input、Procedure、Expected、Failure hints。共同场景的 Failure hints 指向语义阶段，各后端可附加具体实现入口，不要求拥有相同 details 名称。

### 需要补足的边界

1. 结合 C09，为已完成但失败的额外 token 增加非 void/void 场景，明确两端共同预期后再实现。
2. 验证原生 task 的停止、已停止 token、通知到达与取消竞争；不把未定义的竞争胜负固定成断言。
3. 验证原生 operation_state 的存储和最终回收，区分“保留结果”与“保留执行捕获”。
4. 验证公开 Awaitable/Executor 别名在头文件和模块中的相同用法及导出。

现有 Asio 的验证结果是行为基线，不构成 stdexec 后端已经通过的证据。跨后端验收比较共同场景及各配置适用的用例；Debug 的断言测试与 Release 的用例数量可以不同。

## 11. 后续构建接入与标准库切换

源码实现开始后再决定实际文件布局。本目录无需提前创建 include、src、modules、tests 或空 CMake 文件。

构建接入可以保留现有 `lcf::async_asio`，增加 `lcf::async_stdexec`，并由统一 facade target 选择其一。具体 target/option 名称在接入时确定，不改 `task::` 业务接口。

同一 executable 的所有翻译单元使用一致的后端选择；不能混合不同实现的同名 task 类型或两份 `lcf.async` 模块。不同后端、编译器和模块配置使用独立构建产物。

实施顺序：

1. 公共别名、运行环境适配、公开合同测试整理。
2. 固定 stdexec 版本并完成最小执行能力验证。
3. 同步 Chain、启动托管、completion 交付。
4. 异步节点、异常发布、执行捕获释放。
5. 错误恢复、重试及 timer 取消。
6. 结果订阅、junction、独立分支取消。
7. memory_resource、头文件/模块和目标工具链验证。
8. 共同 verify 在 Debug/Release 通过后，再运行示例和有意义的性能对比。

接入标准库时，把实际使用的设施逐项从 stdexec 适配切到 std::execution；对 task/scope/类型擦除等差异保留项目适配。timer、主线程 pump、共享结果、确定性错误选择和兼容错误映射继续保留。

完成条件是公开编译期检查、C01—C14、适用的后端集成测试及资源/生命周期检查通过。标准库版本也执行同一流程，不能以成功包含 <execution> 或开启 C++26 作为切换完成的依据。
