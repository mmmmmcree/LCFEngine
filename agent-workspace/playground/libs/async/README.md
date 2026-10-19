# async

async 是独立维护的 C++23 库仓库。直接在 VS Code 打开本目录即可开发；上一级 `libs` 是库集合入口，通过 Git 子模块引用本仓库。目前两者都保留在 `agent-workspace/playground/libs` 内。

## 打开与构建

本目录拥有自己的 `CMakeLists.txt`、`CMakePresets.json`、编辑器配置和测试入口，不读取上一级目录中的 CMake 文件或示例。集合入口复用本仓库的 presets，`${sourceDir}` 始终对应当前打开的项目根目录。

| preset | 提供的内容 |
| --- | --- |
| `dev` | 两种实现，头文件模式，延迟 verify / sandbox / benchmark |
| `ide` | 两种实现，在主构建中配置 verify / sandbox，便于调试具体用例 |
| `asio` / `stdexec` | 只开发对应实现，延迟测试 |
| `stdexec-ide` | stdexec 的主构建测试 |
| `stdexec-msvc-2026` | stdexec 的 MSVC 2026 模块模式 |
| `stdexec-benchmark` | 只启用 stdexec benchmark |
| `compare` | 两个程序运行相同负载，比较调度、分配、timer 和任务编排 |

在已经初始化编译器环境的终端中：

```powershell
cmake --preset dev
cmake --build --preset debug --target async_sandbox async_stdexec_sandbox
cmake --build --preset debug --target async_verify async_stdexec_verify

cmake --preset ide
cmake --build --preset ide-debug --target async_verify async_stdexec_verify

cmake --preset compare
cmake --build --preset compare-release
```

VS Code 中选择 configure preset、Debug/Release 和构建目标即可。大纲按 `async/asio`、`async/stdexec`、各自的 `tests/verify|sandbox|benchmark` 以及公共 `async/tests/benchmark` 分组。头文件库也登记了自身文件，可以展开浏览。sandbox 承担公开接口的编译和运行检查，没有额外的 compile test。

`dev` 不在配置阶段获取测试框架；构建对应测试目标时才配置独立子构建。`ide` 为逐项发现和调试用例而立即解析 GoogleTest。基础库依赖另行处理：Asio 的默认 FETCH 下载延迟到构建，stdexec 的 FETCH 在配置阶段获取其头文件；均支持本地源码覆盖。

构建目录包含自己的 `.gitignore`，忽略该目录全部内容。删除构建目录后，下一次配置会重新创建忽略规则。

## 目录与公开接口

```text
async/
├─ CMakeLists.txt / CMakePresets.json
├─ cmake/                       库级选项、包导出、开发依赖工具
├─ docs/                        模块使用契约
├─ asio/
│  ├─ deps.json
│  ├─ include/async/core/       Asio 基础类型别名
│  ├─ include/async/task/       Chain、合流和错误处理
│  ├─ modules/
│  └─ tests/                   Asio 适配和实现专属检查
├─ stdexec/
│  ├─ deps.json
│  ├─ include/async/core/       基础类型别名及操作入口
│  ├─ include/async/execution/  协程、调度、定时、同步和取消设施
│  │  └─ 各组件的 details/     队列、帧缓存、服务和等待注册
│  ├─ include/async/task/       任务编排
│  ├─ modules/
│  └─ tests/                   stdexec 专属检查
└─ tests/
   ├─ verify/                  两种实现共用的接口契约
   └─ benchmark/               共用比较负载及资源诊断
```

基础异步能力属于 `core`，公开名称直接位于 `lcf::async`；编排能力位于 `lcf::async::task`。目录区分具体实现，一个程序选择其中一种实现。

两个实现的 `core`、`task` 具有相同的逻辑职责。Asio 的执行设施由第三方提供；stdexec 的实际类型位于本库的 `execution`。`core` 通过类型别名及薄适配提供基础能力入口，不增加另一套运行时状态。stdexec 的 `<async/execution.h>` 可直接用于原生 sender/receiver 开发，公开名称位于 `lcf::async::execution`；这一层独立于 `core` 和 `task`，不要求其他实现提供同名原生接口。

| 入口与名称 | Asio | stdexec |
| --- | --- | --- |
| `core/Awaitable.h`：`lcf::async::Awaitable<T>` | `asio::awaitable<T>` | 本库的惰性协程类型，与 stdexec sender/receiver 互操作 |
| `core/Executor.h`：`lcf::async::Executor` | `asio::any_io_executor` | 本库可投递回调的 scheduler 适配 |
| `core/Context.h`：`lcf::async::Context` | `asio::io_context` | 本库队列与协作式预算调度 |
| `core/timer.h`：`delay(duration)` | 返回 Asio awaitable | 返回定时 sender，可直接 co_await |
| `core/operations.h`：`post(executor, function)`、`yield()` | Asio 投递与协程让出 | 原生 executor 投递与协程让出 |
| `core/Signal.h`：`Signal` | concurrent_channel 的一次触发广播适配 | 原生通知状态与 sender |
| `core/cancellation.h` | `CancellationSource`、`bind_cancellation`、`request_cancellation` | 同名入口，使用 std::stop_source |
| 编排入口 | `async/task.h` | `async/task.h` |

也可以包含 `<async/core.h>` 获取所选实现的基础能力。两个 `Context` 保留各自的方法和语义，例如 Asio 的 `get_executor()` 与 stdexec 的 `getExecutor()`。这里没有增加统一运行时包装层，也没有为 stdexec 的定时 sender 额外构造一个有状态的 Timer 对象。

两边的 core 均为 `Awaitable.h`、`Executor.h`、`Context.h`、`Signal.h`、`timer.h`、`operations.h` 和 `cancellation.h`。`Signal::set()` 只有第一次返回 true，唤醒所有已有等待者，之后的 `wait()` 也能完成；取消一个等待者不触发 signal，也不取消其他等待者。Signal 可以绑定 executor，也可以在等待时使用协程自己的执行环境。

取消入口统一名称，保留原生模型的行为：Asio 的 source 是 `asio::cancellation_signal`，请求是发给当前 slot 的事件，不保存预请求，需遵守原生信号和 slot 的线程安全要求；stdexec 的 source 是 `std::stop_source`，保存停止状态并支持多个 callback。公共用法为每个独立活动链准备 source，启动时绑定 completion，运行中发出请求；取消不会回滚已经发生的副作用。

stdexec 的 `FrameAllocatorScope`、`QueueScheduler`、`DelaySender`、`PollBudget`、`PollReport` 和 `bind_stop_token` 仅从 `lcf::async::execution` 提供，不再在 core 重复导出。Asio 原来的 `core/Timer.h` 与 `lcf::async::Timer` 别名由公共 `core/timer.h` 的 delay 操作取代；需要有状态的原生定时器时直接使用 `asio::steady_timer`。

`execution` 表达执行基础层的职责，组件内的 `details` 表达内部实现边界；两者不是同一种划分。队列节点、帧缓存、定时服务和等待注册等内部名字不承诺稳定 API。模块模式不导出这些内部名字；头文件模式仍可通过显式包含访问它们。

## 外部项目接入

源码、Git 子模块和 FetchContent 都使用本仓库根入口。消费方默认不启用测试或安装，只需选择实现和基础依赖来源：

```cmake
set(LCF_ASYNC_ENABLE_ASIO ON)
set(LCF_ASYNC_ENABLE_STDEXEC OFF)
set(LCF_ASYNC_DEPENDENCY_PROVIDER PACKAGE)
add_subdirectory(external/async)
target_link_libraries(application PRIVATE lcf::async_asio)
```

stdexec 对应 `lcf::async_stdexec`。依赖来源默认 `PACKAGE`，缺包直接报错；`PROVIDED` 要求调用方先提供约定 target；`FETCH` 使用 `deps.json` 中固定的 revision。支持 `FETCHCONTENT_SOURCE_DIR_ASIO`、`FETCHCONTENT_SOURCE_DIR_STDEXEC` 和已经提供的依赖 target。Asio 的延迟下载路径不会执行尚未填充的父级 FetchContent 声明；需要父级改选来源时，先提供 target 或使用源码覆盖。

`LCF_ASYNC_BUILD_TESTS` 限制全部开发内容。各实现再通过 `LCF_ASYNC_ASIO_BUILD_TESTS` / `LCF_ASYNC_STDEXEC_BUILD_TESTS` 及 `BUILD_VERIFY`、`BUILD_SANDBOX`、`BUILD_BENCHMARKS` 选择开发目标。测试框架的来源继承库策略，也可分别覆盖。`PROVIDED` target 无法传给另一个 CMake 构建树，因此这种开发配置应启用对应的 `TESTS_IN_MAIN_BUILD`。

Asio 的版本工具为 `list_dependency_versions` / `update_deps_to_latest`；stdexec 为 `stdexec_list_dependency_versions` / `stdexec_update_deps_to_latest`。它们分别维护所属实现的 `deps.json`，不读取集合级版本清单。

## 安装包

安装时显式设置 `LCF_ASYNC_INSTALL=ON`。公开头文件、选中的库 target 和 CMake 包进入安装树，测试及测试框架不进入安装包。

```powershell
cmake -S . -B build/install -DLCF_ASYNC_INSTALL=ON -DLCF_ASYNC_DEPENDENCY_PROVIDER=PACKAGE
cmake --build build/install --config Release
cmake --install build/install --config Release --prefix path/to/prefix
```

消费方通过组件选择已安装实现：

```cmake
find_package(LCFAsync CONFIG REQUIRED COMPONENTS asio)
target_link_libraries(application PRIVATE lcf::async_asio)
```

stdexec 使用组件 `stdexec`；未指定组件时加载安装包中全部实现。包配置只查找所选组件的公共依赖，不在消费阶段下载第三方源码。Asio/stdexec 依赖包由消费项目或包管理器提供，FETCH 不意味着随 async 安装第三方头文件。

## 阅读与验证

- [stdexec 调度、协作预算及实现](stdexec/README.md)
- [公共接口测试和比较基准](tests/README.md)
- [头文件与模块模式](docs/cpp-modules-transition.md)

本次整理已验证两种头文件实现的公共测试、sandbox、独立源码和移动安装前缀后的包消费，以及独立入口的共享比较目标。stdexec 模块另外验证公开入口和内部名称边界。Asio 原有模块路径在本机 MSVC 2026 完整 verify 中仍有 `asio::config_service` 的 C1116 问题；默认开发配置使用头文件模式。
