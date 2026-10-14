# AsyncTaskChain 与 AsyncTaskFlow 的互操作思路

本文记录下一步的设计方向，尚未实现。以 2026-10-14 的工作区代码为参照。

如果想先理解协程、executor、完成通知这些概念，请先阅读 [现有代码与协程 / Asio 导读](async_task_chain_flow_guide.md)。本文重点回答：如何让整条 Chain 成为一个 Flow 节点，以及这种组合与按操作组织任务有什么关系。

## 1. 要保留的两种能力

`AsyncTaskChain` 适合一次性消费的单向依赖链。每个节点先等待前驱，再执行自己；工作可以跨 executor，但链内数据依赖仍决定先后。

`AsyncTaskFlow` 适合分叉、共享前驱和合并。它必须让一个节点只执行一次，并允许多个协程独立等待同一份结果。

整合目标是共享任务结果与调用适配的规则，并让两者组合。Chain 内部继续使用自己的执行状态；Flow 在复合操作边界上管理整体完成。

| 层次 | 职责 |
| --- | --- |
| 公共任务适配 | 识别普通值、void、error_code、expected、awaitable；转换结果与异常 |
| Chain | 顺序执行一组相互依赖的任务 |
| Flow | 组织依赖拓扑，处理共享启动和完成等待 |
| 互操作适配 | 把整条 Chain 的完成和终值接入一个 Flow 节点 |

当前 Chain / Flow 仍各自维护任务适配代码。这个公共层也属于后续工作，并未抽取。

## 2. 以操作为视角：link 是顺序段，merge 是汇合点

你提出的操作视角可以表达为：

~~~mermaid
flowchart LR
    P["共享前置结果<br/>Flow 边界"] --> A["Chain A<br/>顺序 link"]
    P --> B["Chain B<br/>顺序 link"]
    A --> M["merge<br/>等待 A、B"]
    B --> M
    M --> C["Chain C<br/>后续顺序 link"]
~~~

连续 `link` 可以用 Chain 执行；`merge` 等待多个段；merge 后的连续 link 可以形成新的 Chain。

这里有两个不同维度：

| 位置 | 输入依赖 | 完成结果的消费者 |
| --- | --- | --- |
| 单纯的顺序 link | 一个前驱 | 通常一个后继 |
| merge | 多个前驱 | 可以一个，也可以多个 |
| 共享前驱 / 分叉点 | 可以只依赖一个前驱 | 多个后继 |

所以，只有 merge 需要多依赖等待，但共享前驱也需要“完成一次、通知多个等待者”。执行段的边界应同时考虑汇合与分叉。

**这是一种未来的组织方式。当前 Flow 仍为每个 link 创建自己的 Flow 节点，没有自动将连续 link 融合成 Chain。**

## 3. 第一步：显式把整条 Chain 接入 Flow

可以先提供一个独立的适配入口，下面是使用草案：

~~~cpp
auto image_flow = as_flow(std::move(image_chain), render_executor);
auto shader_flow = as_flow(std::move(shader_chain), render_executor);

auto joined = image_flow.merge(shader_flow)
    .link(gfx_executor, prepare_passes);

std::move(joined).start(render_executor, on_complete);
~~~

`as_flow` 是拟议接口，当前没有这个函数。其契约应当是：

1. 消费一条尚未启动的 Chain，把它的所有权交给返回的 Flow 节点。
2. 转换只建节点；直到外层 Flow start 并请求该节点执行时，才运行 Chain。
3. 保留 Chain 内部每个 link 的工作 executor。
4. Chain 全部完成后，该 Flow 节点发布成功终值或失败。
5. 后续复制 Flow、分叉、merge，都共享这一次 Chain 执行与终值。

草案中的第二个参数设置返回 Flow 的默认 executor，供后续未显式指定 executor 的 link 使用。协调执行由最外层 `start` 决定；这里两处都使用 render_executor，便于阅读。

这里的“一个 Flow 节点”是一个整体完成边界，不等于只创建一个协程或只派发一个 handler。Chain 内部的工作调度与异步等待仍然存在。

如果以后需要根据前驱输入临时构建一条 Chain，可以再增加工厂式接入。它要在前驱成功后构造并消费 Chain；具体接口等结果与所有权协议稳定后再确定。

## 4. 运行过程：Flow 只观察 Chain 的整体完成

~~~mermaid
sequenceDiagram
    participant F as Flow 协调协程
    participant C as Chain 顺序执行
    participant W as CPU / GFX executor
    F->>F: tryStart 成功，取得执行资格
    F->>C: 等待整条 Chain 的运行
    C->>W: co_spawn 第一个可执行任务
    W-->>C: 任务结果
    C->>W: 按依赖继续执行后续任务
    W-->>C: 最终任务结果
    C-->>F: 有所有权的终值视图
    F->>F: 发布完成，通知后继
~~~

在当前 RenderSystem 的安排下，Flow 与 Chain 的协调可以都发生在 render 上下文，实际函数仍分配给 CPU 或 GFX executor。

协调协程等待 Chain，不意味着阻塞 render 线程。遇到真正的异步等待时，它挂起；事件循环可以处理其他工作。同步的 render 任务仍会占用 render 线程，不能因为外面多包了一层 Flow 就获得抢占能力。

Flow 的一次启动与 Chain 的一次运行应形成一个整体完成关系。适配操作只有在 Chain 确实完成后才结束，不能刚调用 `Chain::start` 就返回，否则 Flow 会过早释放捕获或报告完成。

## 5. 怎样让 Chain 成为可等待操作

当前 [Chain::start](../../examples/shader_toy/render_system/src/private_include/AsyncTaskChain.h#L242) 是“启动 + 完成回调”。当前 [Chain::run_node](../../examples/shader_toy/render_system/src/private_include/AsyncTaskChain.h#L178) 本身已经是一个等待整条链执行的协程。

因此有两条接入路径：

| 路径 | 实现含义 | 当前考虑 |
| --- | --- | --- |
| 增加 `run() &&`，返回 Asio awaitable | 消费 Chain，复用其已有 run_node，再返回终值 | 建议优先考虑；完成关系直接，边界不需要另做通知转接 |
| 通过 `async_initiate` 适配 start 回调 | 将 start 建立为 completion-token 异步操作，支持 use_awaitable | 可行；还要处理关联 executor、所有权和异常完成 |

这两条路径都不需要给 Chain 的每个节点增加 Flow 的等待者列表。即使选择回调路径，也只在整条 Chain 的边界做适配。

它们的基础知识分别对应导读中的协程所有权，以及 completion signature / completion token。这里记录的是方案，实际 API 仍待实现时确定。

## 6. 结果传递：不能简单保存回调里的 const 引用

Chain 的完成回调当前接收 `const TaskResult<T> &`。引用指向 Chain 节点保存的结果，其有效性取决于那个节点是否继续存在。

有三种结果交接办法：

| 办法 | 影响 |
| --- | --- |
| 复制终值到 Flow | 对可复制的小对象简单；不适合 move-only 类型，大对象可能有额外成本 |
| 从 Chain 移走终值 | 可以转移独占对象；会改变仍持有该节点的既有结果句柄所观察到的值 |
| 持有不可变终值视图及其所有者 | 保留原结果，不要求复制 T；Flow 必须具备接收这种结果存储的路径 |

建议优先考虑第三种。它能满足“整条 Chain 一次执行、终值被多个 Flow 分支读取”，也不会因为适配而搬空 Chain 的结果缓存。

一个可行的视图类型是：

~~~cpp
using ResultView = std::shared_ptr<const TaskResult<ReturnType>>;
~~~

它可以使用 shared_ptr 的别名构造：控制块继续拥有 Chain 的终点节点，指针则指向该节点内部已经发布的 expected。这样不需要再为 T 分配一份存储。

~~~text
Flow 的结果视图
    ├─ 所有权 → Chain 终点节点
    └─ 读取指针 → 终点节点中的 TaskResult<T>
~~~

即使 T 是 `unique_ptr` 或不能复制的图片对象，也可以向多个后继提供 `const T &`。这表示共享读取结果，不承诺 shared_ptr 等间接持有的数据能被无同步地并发修改。

当前 Chain 的终点操作仍可能持有前驱节点。持有终点视图也可能延长整条链相关结果的生命周期；是否在完成后释放不再需要的操作捕获，需要在实现时结合依赖句柄一起验证。

## 7. run 的入口需要先转移所有权

下面是说明所有权的实现骨架，**不是现有 API，也不是完整可直接落地的代码**；省略命名空间前缀、已消费检查和外围异常处理。

外部入口可以是返回 awaitable 的普通成员函数：

~~~cpp
asio::awaitable<ResultView> run() &&
{
    return run_owned(std::move(m_node_sp));
}
~~~

这个入口没有 `co_await` 或 `co_return`，所以它本身不是协程。调用时立即取走节点所有权，随后 Chain 临时对象即使销毁，也不影响返回的执行对象。

真正等待的协程按值持有节点：

~~~cpp
static asio::awaitable<ResultView> run_owned(NodeSharedPtr node_sp)
{
    co_await run_node(node_sp);
    const auto * result_p = &node_sp->result();
    co_return ResultView {std::move(node_sp), result_p};
}
~~~

先取 `result_p`，再移动所有者，也避免在同一个表达式里同时移动 node_sp 和解引用它。

这个骨架解决了三件事：Chain 被一次性消费；节点在挂起期间存在；返回的结果视图继续拥有节点。它不需要通过阻塞等待完成，也不需要把 Chain 所有内部节点改成可被多个协程独立启动的 Flow 节点。

不要仅把一个使用 `this` 的成员函数改写成协程，再认为 `&&` 能保证生命周期。右值限定只限制调用方式，不会自动把 `*this` 搬进协程帧。

## 8. Flow 还需要一个接收共享结果的边界

当前 [Flow::Node::publish](../../examples/shader_toy/render_system/src/private_include/AsyncTaskFlow.h#L256) 接收 expected 值，并将它存进 `optional<Result>`。这个接口还不能直接接收上一节的 ResultView。

要保留 `AsyncTaskFlow<T>` 的自然类型和“后继接收 const T &”的行为，需要让节点能够：

- 保存自己产生的 expected，或保存带所有权的只读 expected 视图；
- 通过同一个 `result()` 接口提供 `const TaskResult<T> &`；
- 在结果存储就绪后，使用现有完成状态与通知机制；
- 在所有消费者不再需要结果前，持有相应的所有者。

具体存储可以是区分“自有值 / 外部视图”的内部结构，或用一个独立的适配节点方案；这属于实现时需要比较的选择。只在外面加 `shared_ptr`，让用户改为接收 `shared_ptr<T>`，与这里希望保留的接口语义不同。

适配后的操作可以概括为：

~~~text
取得 Flow 节点的执行资格
    → co_await Chain.run()
    → 将有所有权的 expected 视图交给 Flow 的发布路径
    → 标记完成并通知等待者
~~~

发布仍然发生一次。多个后继等待的是 Flow 边界节点，实际 Chain 不会因为每个等待者到来而重新运行。

## 9. 依赖方向：核心 Flow 不必依赖 Chain

拟议的代码组织可以是：

~~~text
公共结果与任务适配工具
    ├─ AsyncTaskChain.h
    └─ AsyncTaskFlow.h

独立的 async_task_interop.h
    ├─ 包含 Chain 与 Flow
    └─ 提供 as_flow 等组合操作
~~~

Flow 核心只需要一个接受通用异步操作 / 结果视图的创建入口，不必识别具体的 Chain 类型。

由于 Flow 的 Node 和节点构造接口是私有的，适配层不能直接操作它们。实际落地时需要提供受控的通用创建入口，或给内部适配工厂有限的访问权限；这里不承诺一个尚未确定的工厂名字。

这样，Chain 和 Flow 可以独立使用，互操作适配承担两者的关联，也不需要重新引入虚基类。

## 10. 错误、类型与调度的兼容点

### 错误传播

Chain 失败时，边界 Flow 节点发布同一个 expected 失败状态；其后继业务任务不执行。与它合并的其他分支按当前 Flow 规则继续完成，merge 等完后再报告失败。

expected 的成功 / 失败标签应完整保留，不能用“错误码是否非零”替代它。异步操作或任务函数抛出的异常，则按公共异常转换规则形成任务失败。

### void 与 monostate

当前 Chain 把无值任务转换成 `monostate`，Flow 使用真正的 void。公共返回类型适配也还没有统一。

第一版适配若保留现有 Chain，可以将 `Chain<monostate>` 明确作为有 monostate 值的 Flow；不能悄悄假定所有 monostate 都是应被删除的占位值。若要统一成 void，需要作为显式接口迁移处理。

同理，统一 expected 的外层解包规则，应核对 Chain 的已有调用点，不要在互操作适配中偷偷改变任务原本的返回类型解释。

### move-only 与默认构造

ResultView 避免的是交接终值时复制 T。它不自动解决 Chain 内部已有 `co_spawn` 的返回类型约束。

若要求 Chain 也支持非默认构造的 T，需要同时核对工作协程的结果传输；Flow 当前采用的 optional 包装可以作为公共适配层的参考。不能只加视图就宣称所有这类返回值已经能运行。

### 调度与取消

适配保持各个 link 的 executor，整体等待继承协调协程的 executor。完成回调路径若使用 `async_initiate`，还要尊重 handler 的关联 executor；不能随意在工作线程直接调用恢复 handler。

第一版不额外承诺全图取消。停止 context、取消某次等待和结束整条 Chain 是不同事件，需要在之后单独定义协议。

## 11. 第二步才考虑自动把连续 link 组织成 Chain

显式互操作稳定后，可以研究让 Flow 根据拓扑把顺序段交给 Chain。

这个优化有一个接口条件：当前 Flow 的任何中间句柄都可能被复制、分叉、单独 start 或读取结果。那些可观察的位置需要保留完成结果和共享执行语义。

例如：

~~~cpp
auto middle = root.link(task_a);
auto end = middle.link(task_b);
auto another = middle.link(task_c);
~~~

`middle` 是共享前驱。不能分别生成两份“root → task_a”的 Chain，让 task_a 因为两个后继而执行两次。

自动分段需要识别这些边界。可研究的办法包括执行前固定拓扑并分析分叉，或通过明确的段构造 / 消费接口表达哪些中间结果不被观察；具体选择仍未决定。

后续优化的顺序是：先把整体完成与所有权做正确，再考虑减少 Flow 节点、等待者登记和调度次数。段数减少也不等于 handler 数量可以静态精确计算。

## 12. 落地前要验证的场景

| 场景 | 必须保持的行为 |
| --- | --- |
| 一个 Chain 适配节点被多个 Flow 分支使用 | Chain 实际只执行一次，每个分支能读到同一终值 |
| 两个独立 Chain 节点合并 | 先启动两路；每路内部仍顺序执行 |
| 同一依赖重复出现在 merge 中 | 不重复启动，也不会丢失完成通知 |
| Chain 某个任务失败 | 内部后继停止，边界失败，merge 等其他分支结束 |
| move-only 终值与已有结果句柄共存 | 适配不搬空原有缓存，没有悬空引用 |
| 不同 start executor 等待同一适配节点 | 唯一执行，等待者在自己的执行上下文继续 |
| 临时 Chain、临时操作闭包被销毁 | 挂起中的操作和结果仍有所有者 |
| context 停止或未来增加取消 | 行为符合明确的协议，不把“停止派发”当作“全部完成” |

目前已确定的是组合方向：**一个 Flow 节点代表整条 Chain 的完成边界，Chain 内部继续使用自己的执行模型。**

待实现时需要定下的是 run / completion-token 接口、Flow 接收结果视图的存储路径、通用节点创建入口，以及 void 等类型语义的迁移范围。自动分段留在显式互操作之后。
