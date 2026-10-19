# async 公共测试

接口契约测试属于 async，运行时适配和实现专属测试属于后端。

```text
async/
├─ tests/
│  ├─ verify/                 同一份接口契约用例
│  │  └─ interface_tests.cmake  给后端测试 target 添加公共源文件
│  └─ benchmark/              同一份比较负载、测量程序和报告脚本
├─ asio/tests/
│  ├─ common/Runtime.h        Asio 驱动适配
│  └─ verify/                 Asio 专属模块桥接及隐私探针
└─ stdexec/tests/
   ├─ common/Runtime.h        stdexec 驱动适配
   └─ verify/                 原生取消竞争、协作预算、scheduler、模块探针
```

## 接口测试如何复用

[interface_tests.cmake](verify/interface_tests.cmake) 维护公共测试源文件清单。两个后端分别调用 `async_add_interface_tests(target)`，链接自己的实现和 GoogleTest，再用各自的 `Runtime.h` 驱动相同用例。

公共用例只依赖任务公开接口和测试适配接口，不包含另一个后端的源码，也不创建一个同时链接两个实现的程序。当前覆盖值和 void 归一化、参数约束、额外 token、单次执行、分叉/合流、失败传播、局部重试、取消、结果身份、capture 生命周期及 memory resource。

新增接口契约放到此处，并进入公共清单。若行为仅对某个实现成立，例如 timer 与 stop 的原生竞争、细粒度出队预算或某条模块导入路径，则留在该后端目录。不要为迎合后端实现而修改共同预期；差异应通过契约或后端适配明确表达。

现有 Asio 入口为 `async_verify`，stdexec 入口为 `async_stdexec_verify`。IDE 配置直接发现两个独立测试程序；延迟配置在构建相应 utility target 时才配置该测试子项目。测试源文件共享与测试依赖是否延迟获取是两个独立问题。

公开入口及使用示例的编译检查由各后端 sandbox 承担，构建并运行 `async_sandbox` 或 `async_stdexec_sandbox` 即可完成验证。

## 比较基准的入口

在 async 仓库根目录或集合入口 `libs`：

```powershell
cmake --preset compare
cmake --build --preset compare-release
```

VS Code 选择 `compare` 配置、Release 和 `async_compare` 目标。默认只提供延迟入口；构建这个目标后才配置 Google Benchmark。两个后端的单独 verify/sandbox/benchmark 关闭，比较本身不需要 GoogleTest。

需要在主工程里直接调试比较程序时，设置 `LCF_ASYNC_BACKEND_BENCHMARKS_IN_MAIN_BUILD=ON`。这时会立即解析 Google Benchmark，提供下列可执行目标：

| 目标 | 测量内容 |
| --- | --- |
| `async_compare_asio` / `async_compare_stdexec` | Google Benchmark 吞吐/总耗时，不替换全局分配函数 |
| `async_diagnostics_asio` / `async_diagnostics_stdexec` | poll 的 p50、p99、最大耗时，回调数量，timer 延迟 |
| `async_memory_asio` / `async_memory_stdexec` | 独立进程中的 C++ new 分配计数和字节量，不用于速度测量 |

`async_compare` 构建并运行全部程序。从 async 打开时，结果默认位于 `build/compare/tests/benchmark/results/Release/`；从集合入口打开时，位于 `build/compare/async/tests/benchmark/results/Release/`。可通过 `LCF_ASYNC_BACKEND_BENCHMARK_RESULTS_DIRECTORY` 改写；Debug/Release 分开存放。

比较测试归 async 所有，各实现作为平等的参测对象。默认控制台输出一张并排耗时表，每行对应一个测试，Asio 与 stdexec 的用时位于相邻列，统一使用微秒；`Winner` 列标记本次该项测试用时较少的实现。每个数值表示一次完整负载迭代，不是单个节点的耗时。

| 输出 | 用途 |
| --- | --- |
| `summary.md` | 默认阅读入口，包含耗时、poll p99 / max、分配次数和峰值存活字节的并排表 |
| `timing.csv` | `case, asio_us, stdexec_us, winner, ...`，保留完整测试名与各自迭代次数 |
| `diagnostics.csv` | 同一指标的 `asio_*`、`stdexec_*` 列相邻，包含全部 poll 和 timer 诊断指标 |
| `memory.csv` | 分配次数、累计请求、峰值存活和 remaining 字节的相邻列 |
| `timing_raw.csv` | 后端逐行的原始耗时输入，保留原单位和成功 / 失败状态 |
| `asio.json` / `stdexec.json` | Google Benchmark 原始结果与环境信息 |
| `*_diagnostics.csv` / `*_memory.csv` | 各后端的原始诊断和分配结果 |
| `*_benchmark.log` / `*_diagnostics.log` / `*_memory.log` | 各测量程序的原始控制台输出 |

报告按完整测试名对齐，诊断和内存数据还按负载大小对齐，不依赖两份文件中的行顺序。所有测试共有的 `/min_time:.../real_time` 后缀只在阅读表中收起，CSV 保留完整名字。优胜者按未舍入的有效用时判断，相等时标记 `tie`。缺失项目显示 `n/a`；失败项目显示 `error` 并使比较目标失败；任一方缺失或失败时，该项不选优胜者。零耗时仍按测量值参与比较。

再次运行会覆盖同配置结果；需要前后对比时先复制一份结果目录。报告程序 `async_comparison_report` 仅使用 C++ 标准库，不依赖 Python。直接运行 `async_comparison_report <结果目录>` 可以用已有 `timing_raw.csv` 和原始诊断文件重新生成阅读表，不重新测量负载；该可执行文件位于比较子构建的 `tests/benchmark/<配置>/` 下。主构建方式下也可单独构建同名可执行目标。

报告的 CTest 用例 `async.benchmark_report` 覆盖双方分别获胜、持平、行顺序变化、缺失项目、不同负载大小、科学计数法和单位换算、零值、失败记录及大整数分配数据；它不依赖 GoogleTest。

## 各负载在测什么

| 负载 | 内容 |
| --- | --- |
| `dispatch_ready` | 预先 post 256 个回调，只测排空阶段，排除入队与创建 context |
| `post` | 创建 context、post 256 次、排空并销毁；包含分配 |
| `chain` | 构造并运行 64 节点 ready chain |
| `fanout` | 同一 junction 分出 256 个后继，先让它们等待源结果 |
| `observers` | 针对同一源结果创建 256 个独立 merge 描述，检查源发布扇出 |
| `timer_ready` / `timer_wait` | 256 个链中分别等待零延迟/1ms timer，包含任务封装成本 |
| `inline_await` | 一个任务内等待 256 个立即返回的子任务，观察后端实际恢复粒度 |
| `yield_await` | 同样循环，每步显式 yield |
| `worker_chain` | 任务在独立 worker context，控制流程在被 poll 的 context |

同一源文件使用相同负载分别编译，任务结果与完成次数均有检查。吞吐负载包含完整生命周期；只有 `dispatch_ready` 明确排除 setup。timer 场景不是孤立 timer 容器的微基准，不可把结果直接解释成 timer 插入复杂度。

`*_diagnostics.csv` 中，callbacks 是每轮平均值，分位数由 8 轮成功出队样本计算，不包含返回零的忙轮询。`max_steps_per_poll` 为 poll 前后业务计数的差；worker 场景同时存在并行执行，不能把该差值解释为 control 直接调用的任务数。

`inline_await` 不预设每个 co_await 都立即继续。原生 `exec::task` 在当前版本的 sender 转换路径上会增加恢复投递；本库在 MSVC/GCC 上通过亲和性门面消除这层重复调度，立即子任务现在可以在同一次 poll 内连续恢复。Clang MS ABI 使用兼容分支，见后端 README。这个用例用于观察差异，不是限制任意用户循环的保证。

## 内存统计的范围

分配探针单独替换 new/delete，覆盖数组、aligned、sized 和 nothrow 入口，按测量轮次标记分配，旧轮次的释放不会破坏新轮次计数。先 warm up，再统计一轮完整负载。

| 字段 | 含义 |
| --- | --- |
| `new_calls` | 该轮 C++ 分配入口调用次数 |
| `new_requested_bytes` | 累计请求字节 |
| `new_peak_live_bytes` | 该轮已记录分配的峰值存活请求字节 |
| `new_remaining_bytes` | 负载返回时仍未释放的本轮请求字节 |

这些字段不包含 malloc、内核资源、allocator 保留容量、RSS 或分配探针自身的额外头部。remaining 非零也不能单独证明泄漏，后台释放、缓存和进程级服务均需进一步分析。

Asio 有自己的协程帧分配入口与回收机制，部分分配不经过替换的全局 new。应把这些值作为“可观察 C++ 分配”指标，不能据此宣称两个后端的完整内存占用比例。后续可增加独立的进程内存与线程资源测量，而不污染现有耗时程序。

## 基准驱动的 stdexec 优化

当前实现与最新结果见[第二轮](#第二轮)。第一轮保留为基线记录；两轮均为本机短时间样本，不是跨平台结论或回归阈值。

### 第一轮

2026-10-19，本机 MSVC 19.44、Release，Asio pin 对应 1.38.2，stdexec pin 见其 deps.json。先保存优化前的可执行文件，再用相同负载构建优化后程序；停止编译和测试后，前后程序交错运行各 3 轮，取完整负载耗时的中位数，单位 µs。Asio 对照程序 SHA256 前后一致。以下是 stdexec 本轮前后对比，跨后端优胜者仍由公共报告逐项独立标记。

| 场景 | stdexec 本轮前 | stdexec 本轮后 | 耗时变化 |
| --- | ---: | ---: | ---: |
| 排空 256 个 ready callback | 5.015 | 5.420 | +8.1% |
| 创建、post 256 次并排空 | 21.626 | 22.270 | +3.0% |
| 64 节点 chain | 253.384 | 160.426 | -36.7% |
| 256 路同 junction 扇出 | 1731.668 | 1276.512 | -26.3% |
| 256 个独立 merge 观察者 | 1827.000 | 1386.958 | -24.1% |
| 256 个零延迟 timer chain | 2935.822 | 2346.263 | -20.1% |
| 256 个 1ms timer chain | 15539.983 | 15440.471 | -0.6% |
| 256 次立即完成子任务 | 101.492 | 84.052 | -17.2% |
| 256 次显式 yield | 338.179 | 277.621 | -17.9% |
| 64 节点 worker chain | 448.704 | 288.412 | -35.7% |

主要变化是同步任务使用独立 sender、内置队列绕过第二层 scheduler 擦除、延迟构造取消异常、根 holder 内嵌。同步 sender 借用正在等待的协程所持有的输入，只擦除调用入口，兼顾稳定存储与模板实例化规模。实现说明见 [stdexec README](../stdexec/README.md#调度与分配成本)。

原始队列没有修改；纯出队和 post 场景本次分别增加约 0.4 和 0.6 µs，不把这两项列作收益。1ms timer 场景主要受等待和 OS 调度影响，0.6% 的差异不足以支持明确的性能结论。所有数字都是本机短时间样本，不是跨平台结论或回归阈值。

可观察 C++ 分配的变化：

| 指标 | 本轮前 | 本轮后 |
| --- | ---: | ---: |
| 64 节点 chain：new 次数 | 715 | 458 |
| 64 节点 chain：累计请求字节 | 340472 | 205384 |
| 64 节点 chain：峰值存活字节 | 239352 | 177736 |
| 256 路扇出：new 次数 | 4630 | 3345 |
| 256 路扇出：累计请求字节 | 1563880 | 1128104 |
| 256 个 timer chain：new 次数 | 6150 | 5382 |
| 256 个 timer chain：累计请求字节 | 2984608 | 3017392 |
| 256 次 yield：累计请求字节 | 662888 | 695736 |

分配次数下降并不保证每项累计字节都下降：timer 和 await 场景的一些 operation / 协程帧变大，表中保留了相应代价。timer 峰值从 2570944 到 2570960 字节，基本持平；本轮所有负载的 remaining 均为零。它们仍不代表 RSS 或完整内存占用。

64 节点链的成功出队次数由 642 降到 578。源发布、等待者通知、节点进入、实际调用及失败传播的协作边界仍保留；诊断负载的 `max_steps_per_poll` 仍为 1。Asio 同一 chain 约为 65 次出队、68 µs，仍比本轮 stdexec 快，恢复粒度也不同。有限次 p99 / max 采样不作为硬时间上限。

本轮头文件 Debug 116 项、module Debug 118 项、两种 sandbox 和报告回归测试通过；两类原生取消竞争测试在两个模式下各重复 100 次。三轮原始数据、固定程序与汇总保存在本机 `build/optimization/`，属于忽略的构建产物。公共比较报告已用无编译负载的第三轮数据刷新。

目前队列是 mutex 保护的 intrusive FIFO，可由多个 producer/consumer 使用。尚未引入第三方队列。候选方案必须先增加相同 producer/consumer 数、批量和唤醒方式的竞争负载；[ReaderWriterQueue](https://github.com/cameron314/readerwriterqueue) 的 SPSC 约束不能直接替换这里的多线程队列，[ConcurrentQueue](https://github.com/cameron314/concurrentqueue) 可作为另一类候选，但仍需测量分配、内存占用、延迟和公平性。

单次 poll 的实测最大值受 OS 抢占影响，有限次采样无法证明硬上限。协作预算测试验证的是停止继续取出、长链分步和通知分步等语义；benchmark 测量这些语义的实际成本，两者各自保留。

### 第二轮

重点消除立即子协程、显式 yield 和链控制中的重复成本。MSVC 19.44 Release，保存第一轮后的固定程序作为本轮前值；停止编译和测试后，前后程序交错运行各 3 轮，取中位数，单位 µs。Asio 对照程序 SHA256 相同。

| 场景 | stdexec 本轮前 | stdexec 当前 | 耗时变化 |
| --- | ---: | ---: | ---: |
| 排空 256 个 ready callback | 5.586 | 5.529 | -1.0% |
| 创建、post 256 次并排空 | 21.545 | 22.363 | +3.8% |
| 64 节点 chain | 152.353 | 54.008 | -64.6% |
| 256 路同 junction 扇出 | 1191.632 | 752.984 | -36.8% |
| 256 个独立 merge 观察者 | 1303.550 | 838.210 | -35.7% |
| 256 个零延迟 timer chain | 2271.910 | 1510.843 | -33.5% |
| 256 个 1ms timer chain | 15083.630 | 15186.525 | +0.7% |
| 256 次立即完成子任务 | 82.193 | 20.626 | -74.9% |
| 256 次显式 yield | 270.323 | 63.139 | -76.6% |
| 64 节点 worker chain | 266.359 | 189.407 | -28.9% |

同期 Asio 的 chain 为 63.073 µs、立即子任务为 9.008 µs、yield 为 97.451 µs。chain 与 yield 的原有倍数差距已消除。纯出队几乎不变，post 增加约 0.8 µs；timer wait 的 0.7% 差异不作明确性能结论。

变化分为三层：

- 用 `Awaitable` 门面传递默认 sticky task 已具备的完成亲和性，避免立即子任务外再加恢复投递。Clang 22 / MS ABI 当时使用返回对象转换兼容分支，不启用这项；其完整头文件契约仍通过。第三轮已用自有返回对象和 promise 替换该门面。
- 当前 scheduler 的显式 yield 只调度一次；实际调用前的 worker 投递也归到一处。测试适配 `testing::yield()` 改成直接返回公共 yield，消除测试额外添加的协程帧；第 9 项收益包含这项适配修正。
- 节点 entry 使用稳定的 `ExecutionSender::Operation`，去掉逐节点 `execute()` 控制协程及相关适配。入口调度失败仍在 control 上发布源结果并通知合流观察者，连接异常也不会留下未完成的源节点。

| 指标 | 本轮前 | 当前 |
| --- | ---: | ---: |
| chain：new 次数 | 458 | 200 |
| chain：累计请求字节 | 205384 | 118344 |
| chain：峰值存活字节 | 177736 | 100056 |
| fanout：new 次数 | 3345 | 2059 |
| timer chain：累计请求字节 | 3017392 | 1890992 |
| inline await：new 次数 | 534 | 270 |
| yield：new 次数 | 1558 | 526 |
| yield：累计请求字节 | 695736 | 143064 |

成功出队次数：chain 578 → 257，inline await 269 → 5，yield 1037 → 261。节点、失败传播、通知、显式 yield 的协作边界保留；inline await 现在可在一次 poll 中推进 256 次内部计数，与任意用户循环本来就需要自行协作的契约一致。

第 8 项剩余差距另做了规模扫描，MSVC Release 三轮中位数：

| 立即子任务数 | Asio (µs) | stdexec (µs) | stdexec new 次数 | stdexec 累计请求字节 |
| --- | ---: | ---: | ---: | ---: |
| 0 | 3.597 | 1.884 | 14 | 7896 |
| 16 | 3.782 | 2.888 | 30 | 10200 |
| 64 | 4.734 | 6.425 | 78 | 17112 |
| 256 | 8.716 | 20.405 | 270 | 44760 |
| 1024 | 24.480 | 76.082 | 1038 | 155352 |

第二轮中，每增加一个只做 `co_return` 的子协程仍新增一次 144 字节的可观察分配；已没有逐子任务的排队。剩余成本包含当时所选 `exec::task` 的帧、默认上下文、创建与销毁。缺少可注入 allocator 是该具体 task 实现与本库复用方式的接口边界；第三轮通过自有 promise 继续优化。尚未用 profiler 精确分摊每个部分的耗时。Asio 在等待期间复用帧，其全局 new 计数不随子任务数量增加；这不代表整个过程没有分配活动。

最终 MSVC 头文件 Debug 123 项、module Debug 125 项、Clang MS ABI 头文件 Debug 123 项及各 sandbox 通过。15 个原生场景在两个 MSVC 模式下各重复 100 次；GCC 头文件编译和 0～1024 子任务扫描通过。Release 比较的全部负载与公共报告回归通过；Release 全量契约套件未重跑。

源码提交依次为 `1192f2a`（第一轮基线）、`e18a832`（亲和性）、`ca42ad9`（单次投递）、`fc76ee0`（Clang 返回对象兼容）、`78c851a`（节点控制状态）。提交只含 stdexec 实现头文件；测试、测试适配、报告和文档继续留在工作区。原始程序、三轮数据、扫描与汇总在本机忽略的 `build/optimization2/`。公共报告已用最终第三轮数据刷新，仍逐项标记双方优胜者。

### 第三轮：自有 Awaitable

`Awaitable` 现在拥有返回对象、promise、帧生命周期及分配钩子，继续通过 stdexec 的公开适配等待 sender、传播 stopped 和连接 receiver。原生 `exec::task` 与它双向互操作，不需要修改第三方源码。

默认帧缓存每线程保留至多 64 KiB 空闲块，另有约 1 KiB 桶元数据。帧头在本机为 16 字节，分桶步长 64 字节，缓存满时淘汰较大的空闲块。自定义 PMR 通过 `FrameAllocatorScope` 在同步创建阶段选择，资源记录在帧头，释放可以发生在别的线程。资源的寿命和线程要求见 [实现说明](../stdexec/README.md#awaitable-与帧分配)。

分配减少后，立即子任务仍有明显的 context 复制成本。同类父子 promise 改成借用父 context；父协程等待期间保持它的寿命。子任务切换 scheduler 时才复制自己的 context，避免修改父 context。外部 promise 接入仍构造本地 context，因为其环境不一定是稳定引用。

MSVC 19.44 Release，基线为 `78c851a`，前后固定程序交错运行各三轮，取中位数，单位 µs。Asio 对照程序 SHA256 相同。

| 场景 | Asio | stdexec 本轮前 | stdexec 本轮后 |
| --- | ---: | ---: | ---: |
| 排空 256 个 ready callback | 47.229 | 5.050 | 5.257 |
| 创建、post 256 次并排空 | 102.278 | 21.653 | 21.542 |
| 64 节点 chain | 64.387 | 53.001 | 52.831 |
| 256 路同 junction 扇出 | 1857.468 | 776.146 | 779.855 |
| 256 个独立 merge 观察者 | 1901.758 | 865.728 | 879.301 |
| 256 个零延迟 timer chain | 1444.440 | 1446.127 | 1478.758 |
| 256 个 1ms timer chain | 15271.370 | 15232.962 | 15213.570 |
| 256 次立即完成子任务 | 8.871 | 20.246 | 8.138 |
| 256 次显式 yield | 95.074 | 62.408 | 50.945 |
| 64 节点 worker chain | 235.190 | 191.138 | 194.398 |

立即子任务耗时下降约 59.8%，yield 下降约 18.4%；chain 接近持平。其余场景没有明确收益，部分略慢；纯出队代码没有变化，其约 0.2 µs 差异也提示了测量波动。公共比较报告仍使用同一项为行、Asio/stdexec 相邻列及该项 winner；该报告展示最终第三次运行，以上表格展示三轮中位数。

| 场景 | 可观察 new 次数：前 → 后 | 累计请求字节：前 → 后 |
| --- | ---: | ---: |
| chain | 200 → 161 | 118344 → 62600 |
| fanout / observers | 2059 → 1932 | 679256 → 683752 |
| timer chain | 3078 → 2956 | 1890992 → 1967408 |
| inline await | 270 → 13 | 44760 → 7352 |
| yield | 526 → 269 | 143064 → 44216 |
| worker chain | 202 → 200 | 118392 → 124216 |

分配次数减少不代表每类负载的申请字节都下降。fanout 约增加 0.7%，timer 约增加 4.0%，worker 约增加 4.9%。chain 的可观察峰值由 100056 降至 44312 字节，测量结束仍有 43264 字节来自空闲缓存。`new_remaining_bytes` 可以包含缓存原始存储；对象和参数已经析构。统计只观察 C++ new 请求，不是 RSS 或全部分配，预热阶段留下的块在后续复用时不会再次计入。

立即子任务另做相同规模扫描，三轮中位数：

| 子任务数 | Asio (µs) | stdexec (µs) | stdexec new 次数 | stdexec 累计请求字节 |
| --- | ---: | ---: | ---: | ---: |
| 0 | 3.696 | 1.722 | 10 | 1336 |
| 16 | 3.726 | 2.070 | 10 | 1336 |
| 64 | 4.833 | 3.303 | 10 | 1336 |
| 256 | 9.027 | 8.386 | 10 | 1336 |
| 1024 | 25.065 | 28.049 | 10 | 1336 |

预热后，增加子任务不再增加可观察的 new 请求；每个协程的创建、返回及帧回收仍有成本。独立扫描和整个负载集合之间的缓存起点不同，因此它的固定次数 10 与集合中的 inline await 13 有差异。1024 个子任务时 stdexec 仍略慢，不能把 256 个子任务的结果外推到所有规模。

本轮没有减少协作式出队边界。chain / inline await / yield 出队次数分别保持 257 / 5 / 261；节点、通知、失败传播及显式 yield 的 `max_steps_per_poll` 仍为 1，立即子任务内部计数仍可一次推进 256。

生命周期回归还发现并修复了通知与 timer 的取消 callback 注销顺序：最终 receiver 可以立即销毁 stop source，callback 必须先注销。预取消 token 的构造期间回调由注册 gate 延后处理。四种直接 receiver 场景使这个问题可以确定性复现，不依赖调度碰巧触发断言。

最终验证为 MSVC 头文件 Debug 137 / Release 133、模块 Debug 139、Clang 22 MS ABI ASan 137 及各自 sandbox；30 个聚焦场景在模块模式重复 100 次，ASan 重复 20 次。GCC 16 完成头文件编译与 0～1024 子任务扫描，未执行其完整契约套件。报告回归通过，本轮未重跑安装消费矩阵。

源码提交为 `727d472`（自有 promise）、`4c387c9`（终结前注销 callback）、`df172f8`（帧缓存和 PMR 入口）、`7b4523e`（紧凑帧头与分桶）、`492dddc`（缓存满时淘汰）、`17fb189`（借用父 context）。提交只含 stdexec 实现头文件；测试、CMake、报告和文档未提交。原始程序、三轮测量、上下文借用前后扫描及汇总在忽略的 `build/optimization3/`。

### execution 分层验证

stdexec 的实际类型与执行设施现位于 `execution`，`core` 提供类型别名和操作引入。公共接口套件及 sandbox 继续使用 `core` / `task`；原生 sender 场景直接通过 `<async/execution.h>` 使用执行基础层，验证两组入口在同一套任务编排中互通。

本次分层及 core 补齐通过 MSVC 19.44 头文件 Debug：Asio 108 项、stdexec 145 项；VS 2026 stdexec 模块 Debug 149 项，以及各自 sandbox。新增八个公共 core 场景，覆盖 post/yield 的排队边界、绑定及未绑定 executor 的通知广播、并发注册、单个等待者取消及 delay 取消。测试 Runtime 直接使用库的公共操作，不再自行实现 Asio 的 yield、delay 或 Gate。模块套件还增加了帧分配辅助名称和内部队列未导出的两个检查；跨翻译单元函数分别以原生 Executor 和 core 别名声明，验证类型身份。

另从本地依赖包配置并安装两种头文件库，通过干净安装前缀的 `find_package(LCFAsync)` 构建三个 Release 程序：Asio、stdexec 的 core / task 消费样例，以及只包含 `<async/execution.h>`、使用原生 Context、Awaitable、yield 和 delay 的程序。三者均得到结果 42，执行基础层不依赖 core 或 task 头文件。验证脚本与日志位于忽略的 `build/check_execution*.ps1`、`build/core-*.log`。

Asio 模块接口自身能生成，但完整消费测试仍被已有的 MSVC C1116 阻断，诊断涉及 `asio::config_service` 的 IFC 导入；本次没有宣称 Asio 模块完整通过。其默认头文件配置已完成上述公共验证。

本次只拆分源码职责，没有调整调度边界、分配策略或错误传播，也没有重新采样性能；上面的性能表保留其原测量范围。
