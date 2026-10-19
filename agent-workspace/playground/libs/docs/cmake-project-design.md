# 独立库、集合仓库与 CMake 依赖设计

讨论与调研日期：2026-10-18；实施状态更新：2026-10-19。当前集合入口保留在 `playground/libs`，async 作为本地 Git 子模块独立维护，并拥有自己的开发入口。两个实现均提供依赖来源策略和安装导出，公共测试及比较基准位于 async 仓库内。独立开发见 [async README](../async/README.md)，三种外部接入方式见 [消费样例](../examples/async_consumer/README.md)。

## 1. 需求与设计目标

async 应能独立作为 Git 仓库开发，也能加入其他项目。集合仓库当前保留目录名 `playground/libs`，后续可作为 `lcf_cpp_libs` 仓库进入 LCFEngine；它通过 Git 子模块组合多个独立库，在集合级 `examples/` 中组合任意已启用库，逐步替换 LCFEngine 当前 `libs` 的集合职责。

| 需求 | 目标行为 |
| --- | --- |
| 独立开发 | 只打开 async 仓库即可配置、修改、测试、调试和运行 sandbox、benchmark |
| 源码集成 | 支持 Git 子模块、本地源码和 FetchContent；业务 target 使用相同的链接接口 |
| 安装包消费 | 支持安装与 CMake 包导出，使用方通过 `find_package()` 和命名空间 target 消费 |
| 多实现 | async 选择 Asio 等子实现；允许多个实现参与同一次构建，适用于类似多后端 RHI 的组织方式 |
| 按需依赖 | 未启用组件不引入其第三方依赖；测试和 benchmark 的依赖分别隔离 |
| 外部依赖管理 | LCFEngine 可以统一通过 vcpkg 提供依赖，库不接管父项目的包管理器 |
| 一致结构 | 各库遵守相同的目录职责、选项层级和导出契约，内部实现允许不同 |
| 简洁配置 | 常见使用只设置少量整体选项或选择 preset，单项覆盖作为高级配置 |

这些规则约束各实现的配置职责。基础类型位于 `core`，公开于 `lcf::async`；任务编排位于 `task`。Asio 的执行设施由第三方提供，stdexec 的实际类型与设施位于本库的 `execution`，`core` 通过别名和操作引入选择这些能力；内部实现按协程、调度、定时、同步和取消划分。这是实现内部分层，没有创建跨实现的统一运行时抽象层，各实现保留自身语义。

## 2. 从最终使用方式确定接口

下列配置描述配置契约。源码接入使用独立 async 仓库的根入口；源码、FetchContent 和安装包消费均以同名公开 target 链接。

### 独立开发

在仓库根目录选择开发 preset。preset 负责选择编译器、构建模式和开发内容，无需用户逐层开启默认选项。

- 常规开发 preset 启用 Asio、verify 和 sandbox，依赖来源选择 `FETCH`。
- benchmark preset 在开发配置上开启 benchmark，才解析 Google Benchmark。
- 保留按目标配置测试项目的能力；需要直接发现和调试测试用例的 IDE preset 可选择在主构建中配置测试。
- 当前 playground 宿主通过 preset 明确提供开发配置，不让子库猜测宿主身份。

### 集成到 LCFEngine 或集合仓库

添加方选择实现、关闭开发内容并提供依赖来源。典型配置只需以下设置：

```cmake
set(LCF_ASYNC_ENABLE_ASIO ON)
set(LCF_ASYNC_BUILD_TESTS OFF)
set(LCF_ASYNC_DEPENDENCY_PROVIDER PACKAGE)
add_subdirectory(external/libs/async async-build)
target_link_libraries(application PRIVATE lcf::async_asio)
```

vcpkg 的 toolchain、triplet、版本和安装清单由添加方管理。async 通过标准 `find_package()` 获取 target，不需要 `USE_VCPKG` 或 `IMPORT_AS_SUBMODULE` 选项。

Git 子模块仅决定源码如何获取；本地 `add_subdirectory()` 与 FetchContent 采用同一套库配置。

### 消费安装包

安装包提供与源码接入相同的公开 target，不加载测试目录或开发工具：

```cmake
find_package(LCFAsync CONFIG REQUIRED)
target_link_libraries(application PRIVATE lcf::async_asio)
```

安装包保存生成时确定的实现和构建模式；消费方不通过修改构建选项重新选择已经安装的实现。

## 3. 目录与选项的所有权

每层声明自身整体策略，并控制直接子组件是否参与配置。具体组件声明自己的局部参数。选项名表达所属库、实现和行为，不机械拼接所有目录名。

| 声明位置 | 配置职责与目标选项 |
| --- | --- |
| 集合仓库 | `LCF_LIBS_ENABLE_ASYNC` 选择库，`LCF_LIBS_BUILD_EXAMPLES` 管理跨库示例 |
| `async/` | `LCF_ASYNC_ENABLE_ASIO` 等实现选择；`LCF_ASYNC_USE_MODULES`、`LCF_ASYNC_BUILD_TESTS`、`LCF_ASYNC_DEPENDENCY_PROVIDER`、`LCF_ASYNC_INSTALL` 等整体策略 |
| `async/asio/` | Asio 实现特有行为、`LCF_ASYNC_ASIO_BUILD_TESTS`，以及 Asio 依赖来源覆盖 `LCF_ASYNC_ASIO_PROVIDER` |
| `asio/tests/` | `LCF_ASYNC_ASIO_BUILD_VERIFY`、`LCF_ASYNC_ASIO_BUILD_SANDBOX`、`LCF_ASYNC_ASIO_BUILD_BENCHMARKS`、`LCF_ASYNC_ASIO_TESTS_IN_MAIN_BUILD`；共享 GoogleTest 的解析 |
| `tests/benchmark/` | benchmark 自身参数、Google Benchmark 的解析及 `LCF_ASYNC_ASIO_BENCHMARK_RESULTS_DIRECTORY` |

没有实际独立选择需求时，不增加选项。未来实现使用同样规则；统一抽象层若存在，应作为明确组件声明其依赖，不能根据启用的后端数量自动推断。

多个实现采用独立布尔选项，不使用互斥的单一 `BACKEND` 枚举。多个实现能否同时链接进同一程序，仍取决于接口、符号和命名模块的设计，不能仅由构建开关保证。

### 开关与默认值

- 整体开关限制下层能力。父层 `OFF` 关闭对应分支，父层 `ON` 允许下层选择，不强制所有子组件开启。
- 例如配置 Asio verify，需要实现启用、async 允许测试、Asio 允许测试和 verify 启用四个条件同时成立。
- `BUILD_TESTS` 是当前 `tests/` 子树的总开关，包含 benchmark；benchmark 不依赖 verify 或 sandbox。仅运行 benchmark 时通过专用 preset 表达这一组合。
- `BUILD_* = ON` 表示提供相应目标；是否加入默认构建、是否立即解析开发依赖和是否自动执行，由目标及测试配置方式另行确定。
- 源码被引用时默认关闭开发内容；独立开发由 preset 提供完整配置。明确传入的值优先，不使用 `CACHE FORCE` 重置子层选择。
- 不可用的选项在 GUI 中隐藏并保留原选择，重新启用时恢复。布尔条件选项优先使用 `cmake_dependent_option()`；不通过删除缓存实现联动。

`LCF_ASYNC_USE_MODULES` 作为整体接口模式，不为每个实现增加重复覆盖。相关组件需要一致配置，类型归属与兼容范围遵守 [头文件与 C++ modules 的双模式方案](cpp-modules-transition.md)。

## 4. 依赖来源

保留三种明确模式，不提供自动查包失败后下载的默认回退：

| 模式 | 约定 |
| --- | --- |
| `PROVIDED` | 添加方预先提供约定 target；缺失时报错，不查包、不下载 |
| `PACKAGE` | 复用已有兼容 target，否则通过 `find_package()` 查找；缺失时报错，不回退到源码下载 |
| `FETCH` | 通过源码依赖声明获取，默认固定 revision，支持父项目和本地源码覆盖 |

库级 `LCF_ASYNC_DEPENDENCY_PROVIDER` 默认 `PACKAGE`。独立开发 preset 将它设为 `FETCH`，一般无需逐项配置。

依赖的直接使用目录拥有单项覆盖，例如 `LCF_ASYNC_ASIO_PROVIDER`、`LCF_ASYNC_ASIO_GTEST_PROVIDER` 和 `LCF_ASYNC_ASIO_BENCHMARK_PROVIDER`。这些属于高级配置，空值表示继承库级策略；每次配置计算有效值，避免把父默认值复制成永久失去联动的子缓存值。

所有来源最终提供相同的依赖 target，例如 `asio::asio`、`GTest::gtest_main` 和 `benchmark::benchmark`。依赖必须满足声明的版本或能力要求；只检查 target 名称存在不足以证明兼容。

源码获取与 target 适配分别处理。对于上游源码不直接提供所需 CMake target 的依赖，可由所属实现目录提供薄适配；`PACKAGE` 路径则使用包本身导出的 target，不重建包管理器已经提供的目标。

父项目可以统一提前解析共享依赖，也可通过本地源码覆盖控制来源。stdexec 另外支持 CMake 原生的 FetchContent 声明及 dependency provider；Asio 保留按构建延迟下载，未填充的父级 FetchContent 声明不参与这条延迟路径，父项目应先提供 target 或源码覆盖。`FETCH` 的固定 revision 是默认来源，不绕过添加方明确的覆盖策略。同名依赖不能重复获取或静默替换；来源或兼容要求冲突时明确报错。

这里的 `*_PROVIDER` 是库的来源策略，不是自行注册 CMake 全局 dependency provider。后者属于添加方选择，子库不修改父项目 toolchain、不注册全局 provider，也不强制覆盖第三方全局选项。

## 5. 声明、筛选与解析

```text
目录选项与无副作用的依赖声明
    -> 计算有效组件及必要依赖
    -> 按来源策略提供依赖 target
    -> 配置实现与已启用的开发内容
    -> 按公共接口生成安装导出
```

各目录保有自身依赖声明。共同需要的选项声明可以提取到本目录的纯声明文件，供正常配置和父级依赖收集复用，不维护两套不同的条件。

声明阶段不下载、查包或调用第三方 `add_subdirectory()`。筛选阶段必须考虑完整的父子开关与来源策略，不能因为递归扫描到某份清单就安装其所有依赖。集成仓库区分主动选择的库与作为依赖引入的库，后者默认不启用自己的开发内容。

| 有效启用条件 | 解析内容 |
| --- | --- |
| Asio 实现启用 | Asio 和所需系统依赖 |
| verify 或 sandbox 启用 | GoogleTest |
| benchmark 启用 | Google Benchmark |
| 仅消费库 | 不解析任何开发依赖 |

功能关闭后，async 不查找、下载、配置或导出该功能专属的依赖，也不创建相关第三方 target。父项目或其他组件已经引入的共享 target 不由 async 隐藏或删除。

按目标延迟配置开发项目时，相应依赖也延迟到该项目实际配置时解析。子构建必须继承所选来源策略和必要搜索信息，不能切回隐式下载。

`PROVIDED` 的 target 属于创建它的 CMake 构建树，不能直接传给独立配置的子构建。使用此模式的开发配置应将相关测试放入主构建；不能只传递 target 名称就假定子构建也已获得依赖。

### 对接 LCFEngine 与 vcpkg

LCFEngine 当前在添加库目录之前收集依赖并调用 vcpkg 安装。该收集过程必须使用同一套有效条件；仅在库中跳过测试目录不足以避免提前安装 GoogleTest。

- 提交给包管理器的依赖仅包含有效组件实际需要、且选择 `PACKAGE` 来源的包。
- `PROVIDED` 和 `FETCH` 不因本库的声明而再次触发包管理器安装。
- 其他库仍可能需要同一包，集合层对全部有效声明合并并检查兼容要求。
- 父级安装完成后，子库仍需通过 `find_package()` 获得 CMake target；安装了文件不等于 target 已经存在。

若后续使用 vcpkg 标准 manifest 自动安装流程，可用 features 分组控制 tests、benchmark 等可选依赖。相关选项和 `VCPKG_MANIFEST_FEATURES` 必须在首次 `project()` 前确定，不能期待子目录中的后置判断撤销已经发生的安装。

## 6. 安装与导出边界

源码获取、库安装和附带第三方文件分别处理。基础方案只安装 async，生成的包配置查找其公共依赖，不在消费时下载源码。

- `lcf::async_asio` 通过 target usage requirements 传播 Asio、必要编译要求及系统链接要求。
- Asio 出现在公共接口中，可以由外部提供，但不能从消费要求中省略。
- 包配置通过 `find_dependency()` 恢复必要依赖，不写入构建机器上的 vcpkg、源码或构建目录绝对路径。
- verify、sandbox、GoogleTest 和 Google Benchmark 不进入公开导出与运行库安装依赖。
- 消费安装包不受使用方同名开发开关影响，导出集合由安装包自身的有效构建配置确定。
- 模块产物受编译器、标准库及构建配置约束；不承诺 BMI 跨工具链通用。

随安装包附带第三方文件记录为扩展需求。后续按依赖明确支持范围、安装规则、许可证和消费方式；不复制整个 vcpkg 安装树，也不提前提供无法保证行为的通用 `INSTALL_DEPENDENCIES` 开关。`FETCH` 已满足独立源码构建的依赖获取需求，不自动意味着再分发这些文件。

## 7. 验证目标

| 场景 | 必须观察到的行为 |
| --- | --- |
| 独立开发 | 仓库内可完成构建、verify、sandbox；已选择的模块模式配置一致 |
| 库消费且关闭测试 | 不解析测试框架，不出现开发 target 或开发依赖导出 |
| 仅启用 benchmark | 获取 Google Benchmark，不因 benchmark 本身引入 GoogleTest；明确关闭第三方自身测试 |
| 仅启用 verify 或 sandbox | 获取 GoogleTest，不获取 Google Benchmark |
| `PACKAGE` 缺失 | 配置失败并指出所需包，不自动联网下载 |
| `PROVIDED` | 复用父级 target；缺失或兼容条件不满足时明确报错 |
| `FETCH` 与本地覆盖 | 默认来源固定；覆盖源码可用于修改调试，不被下载步骤覆盖 |
| 组件关闭后重新开启 | 功能关闭期间无新增依赖；恢复时保留用户配置 |
| 多库共享依赖 | 一份明确来源，无重复 target 或静默版本替换 |
| 安装后独立消费 | 在新的构建目录使用 `find_package()`；移动安装前缀后仍可使用，不需要测试框架 |

## 8. 调研参考

以下官方文档和项目源码已于 2026-10-18 联网核对。CMake 机制以当前项目最低版本 3.28 的文档为依据；项目分支链接可能继续变化，实施时应针对采用的版本复核。

| 参考 | 已核对内容与采用方向 |
| --- | --- |
| [CMake Using Dependencies Guide](https://cmake.org/cmake/help/v3.28/guide/using-dependencies/index.html) | 源码与安装包通过相同命名空间 target 消费；依赖获取可由添加方控制 |
| [CMake FetchContent](https://cmake.org/cmake/help/v3.28/module/FetchContent.html) | 父级先声明优先、本地源码覆盖、查包协作；复用原生机制而非建立独立下载覆盖体系 |
| [CMake dependency providers](https://cmake.org/cmake/help/v3.28/command/cmake_language.html#dependency-providers) | 全局 provider 只能在首次 `project()` 的特定阶段注册，由添加方掌握 |
| [CMakeDependentOption](https://cmake.org/cmake/help/v3.28/module/CMakeDependentOption.html) | 条件布尔选项隐藏并保存原值；GUI 隐藏与不解析依赖是分别实现的契约 |
| [CMake Importing and Exporting Guide](https://cmake.org/cmake/help/v3.28/guide/importing-exporting/index.html#creating-relocatable-packages) | 可迁移导出、公共依赖 target 和 `find_dependency()` |
| [Boost 集合构建](https://github.com/boostorg/cmake/blob/develop/include/BoostRoot.cmake) | 选择库并收集依赖；主动选择的库、必要依赖和测试依赖分别处理 |
| [fmt 构建入口](https://github.com/fmtlib/fmt/blob/master/CMakeLists.txt) | 开发与安装选项分离，独立构建和子项目使用不同默认值 |
| [Abseil 构建入口](https://github.com/abseil/abseil-cpp/blob/master/CMakeLists.txt) | GoogleTest 可由父级提供、查包或源码获取；仅在需要测试设施时解析 |
| [gRPC 的 Abseil 接入](https://github.com/grpc/grpc/blob/master/cmake/abseil-cpp.cmake) | 按依赖选择源码或 package、复用现成 target；获取与随包安装有各自规则 |
| [spdlog 构建入口](https://github.com/gabime/spdlog/blob/v1.x/CMakeLists.txt) | 外部与内置 fmt 的选择；测试、benchmark 和安装分别配置 |
| [vcpkg CMake 集成](https://learn.microsoft.com/en-us/vcpkg/users/buildsystems/cmake-integration) | 标准查包、manifest features 与首次 `project()` 前的配置时机 |
| [vcpkg Asio 包配置](https://github.com/microsoft/vcpkg/blob/master/ports/asio/asio-config.cmake) | 实际提供 `asio::asio`，可与源码接入约定对齐 |

以上参考用于确定配置契约，不逐项复制上游选项。最终以使用方是否能通过少量明确设置完成开发、集成和消费来评估设计。
