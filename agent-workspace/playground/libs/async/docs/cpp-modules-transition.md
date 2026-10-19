# 头文件与 C++ modules 的双模式方案

讨论日期：2026-10-18；实施状态更新：2026-10-19。本文记录已确定的设计方向及 Vulkan-Hpp 参考实现。新 stdexec 后端已按“全局依赖片段 + 命名模块内包含实现 + 条件 export”落地，见其 [模块入口](../stdexec/modules/async.cppm) 和 [使用说明](../stdexec/README.md)。Asio 仍使用后文介绍的旧版导出清单路线，未在本轮迁移。

## 1. 已确定的兼容范围

采用“一份实现、两种构建模式”的方案，优先参考新版 Vulkan-Hpp：工程统一选择头文件模式或命名模块模式。

- 两种模式提供相同的公开源码接口与行为契约。
- 保留 `<async/task.h>` 入口，由构建配置决定包含实现头文件还是 `import lcf.async;`。
- 不要求同一程序中，一部分翻译单元通过头文件定义 async 类型，另一部分通过模块获得 async 类型，并在两者之间传递对象。
- 不承诺两种模式的类型身份、符号名称或 ABI 相同。切换模式后，重新编译使用这些接口的组件。

模式统一约束的是 async 自身的声明。模块消费者仍然可以按需包含标准库、Asio 等依赖的头文件；这些依赖不必同时迁移成命名模块。

迁移以程序及其通过 async 公开类型相连的组件为单位。一个构建工程可以分别生成使用两种模式的独立测试程序。

## 2. Vulkan-Hpp 为什么值得参考

Vulkan-Hpp 从 Vulkan XML API Registry（`vk.xml`）读取 API 数据，通过同一个生成器输出 C++ 头文件与模块接口：

```text
vk.xml → API 数据模型 ┬→ vulkan*.hpp
                     └→ vulkan.cppm
```

`generateCppmFile()` 使用模块模板生成 `.cppm`。旧版通过 `generateCppModuleUsings()`、`generateCppModuleRaiiUsings()` 等函数生成 C++ 名称的导出清单；新版通过头文件中的条件 `export` 导出 C++ 声明，模块模板仍会填入版本号、C API 函数指针导出等内容。

值得参考的是共享实现、组织依赖和控制导出的方式。自动生成服务于 Vulkan API 的规模，并不是双模式方案成立的前提。async 当前可以手写实现头文件和薄模块入口，无须增加 API 描述文件或扫描 C++ 源码的生成器。

## 3. 旧版与新版的关键差异

以下比较基于已核对的 Vulkan-Hpp `v1.4.328`、本机 SDK 的 `VK_HEADER_VERSION == 350`，以及上游提交 `2d6804b76eefce6039e6afb8f9da7bba0f499043`（头文件版本 365）。

### 旧版：全局模块中的声明，加导出清单

Vulkan-Hpp `v1.4.328` 的结构可简化为：

```cpp
module;

#include <vulkan/vulkan.hpp>

export module vulkan_hpp;

export namespace vk {
using vk::Buffer;
using vk::Device;
}
```

C++ 声明来自全局模块片段，类型仍属于全局模块。模块通过 `using` 引入已有实体，公开名称由导出清单决定。

这适合保留头文件与模块消费者之间的类型身份，但仍要求各翻译单元的定义和配置一致，并需要编译器验证。

### 新版：在命名模块内部包含同一份 C++ 头文件

本机 SDK 的 `vulkan.cppm` 在全局模块片段中包含 C API 和宏配置，再在 `export module vulkan;` 后包含 C++ 头文件。省略诊断与其他头文件后，其结构如下：

```cpp
module;

#define VULKAN_HPP_CXX_MODULE 1
#include <cassert>
#include <cstring>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_hpp_macros.hpp>

export module vulkan;
export import std;

#include <vulkan/vulkan.hpp>
```

其中，`VULKAN_HPP_EXPORT` 在模块模式下展开为 `export`，在头文件模式下展开为空。头文件使用 `VULKAN_HPP_EXPORT namespace ...` 等形式复用声明，并在模块模式下跳过普通标准库头文件的包含。

上游已核对的提交使用 `import std;`，本机版本使用 `export import std;`。前者不向消费者承诺重新导出标准库，集成时应以实际采用的版本为准。

C++ 声明位于命名模块内部，通常归属于该命名模块。普通头文件模式下，相应声明则属于全局模块；不能因为两种模式使用相同源码，就认为其类型可以直接混用。

| 对比项 | 旧版路线 | 新版路线 |
| --- | --- | --- |
| C++ 实现头文件的位置 | `export module` 之前的全局模块片段 | `export module` 之后 |
| C++ 声明的归属 | 全局模块 | 命名模块 |
| 公开接口的导出 | 额外的 `using` 清单 | 声明中的条件 `export` |
| 适合的过渡目标 | 保留头文件与模块之间的类型身份 | 工程统一选择一种模式，共享实现源码 |

本项目已明确不要求跨模式混用，因此选择新版路线作为后续调整依据。

## 4. async 当前状态与目标结构

当前 [async.cppm](../asio/modules/async.cppm) 在全局模块片段中通过 `LCF_ASYNC_FORCE_HEADERS` 包含 `<async/task.h>`，随后显式导出 `Chain`、`ChainJunction` 等名称。这与旧版 Vulkan-Hpp 的路线接近。

当前 [task.h](../asio/include/async/task.h) 已具有条件导入入口，[CMakeLists.txt](../asio/CMakeLists.txt) 已通过 `LCF_ASYNC_USE_MODULES` 选择构建模式。后续可以沿用这两个使用约定。

目标结构如下；文件划分和宏名为设计示意：

```cpp
module;

#include <async/task/dependencies.h>

export module lcf.async;

#define LCF_ASYNC_BUILDING_MODULE 1
#include <async/task/ChainJunction.h>
#undef LCF_ASYNC_BUILDING_MODULE
```

Asio 后续调整需要处理以下内容；stdexec 已采用该组织方式：

1. **拆分外部依赖与自身声明。** 全局模块片段只预先包含标准库、Asio 等依赖，自身类型与实现头文件在命名模块内部展开。目前 `common.h` 同时包含依赖、`Result` 等公开别名和内部工具，需要先拆分；不能在全局模块片段提前包含整个 `common.h`，否则其中的声明已经属于全局模块，包含保护还会阻止其在后续再次展开。
2. **对公开接口使用条件导出。** 增加类似 `LCF_ASYNC_EXPORT` 的内部宏，构建模块接口时展开为 `export`，普通头文件模式下展开为空。公开模板、类型别名、函数和必要前置声明统一处理。按公开声明或声明组控制导出，避免把整个含有 `details` 的 namespace 一并导出。
3. **保持内部实现对编译器可达。** 模板实例化依赖的定义仍需保留在模块可达范围内。内部名称没有导出，不影响编译器使用这些定义；名称是否能被消费者直接查找，是另一项约束。
4. **保留兼容入口。** 普通消费者包含 `<async/task.h>` 时，按构建配置选择导入模块或包含实现。模块自身直接包含声明头文件，避免经由兼容入口递归导入自身。当前 `LCF_ASYNC_FORCE_HEADERS` 的必要性可在调整时重新评估。
5. **统一构建配置。** 模式宏由 CMake target 传播，影响公开接口或 ABI 的其他宏保持一致。模块编译完成后，消费者在 `import` 前定义宏，不能重新配置已经生成的模块。

Asio 可以继续留在全局模块，沿用模块构建中已有的 `ASIO_SEPARATE_COMPILATION` 与单独运行时编译方式。async 自己的类型是否归属于命名模块，与 Asio 是否采用独立编译，是两个分别配置的问题。

兼容头文件中嵌入 `import` 对包含位置有要求：普通 `.cpp` 应在全局作用域使用；其他命名模块应在自己的模块声明之后安排导入，不能在 `module;` 全局模块片段中通过该入口展开命名模块导入。

## 5. 工具链与验证方向

C++20 引入命名模块；async 当前已经以 C++23 构建并使用 `std::expected`。参考新版 Vulkan-Hpp 不意味着必须同时采用标准库模块：先通过全局模块片段包含标准库头文件，也可以实现上述结构。是否启用 `import std;`，应单独根据编译器、标准库与 CMake 支持情况决定。

Vulkan-Hpp 的模块接口仍标记为实验性，具体工具链要求应查阅对应版本文档。本次结论基于源码核对，未执行 Vulkan-Hpp 或调整后 async 的跨编译器验证。

后续实现的验证重点为：

- 用同一套公开接口与行为测试，分别构建并运行头文件模式和模块模式；内部工具测试可以只在适合的构建模式下运行。
- 在模块模式下验证 `Chain`、`ChainJunction`、错误处理和公开别名的实际使用，覆盖模板实例化、重载、约束及协程交互。
- 保留内部名称不可直接查找的编译失败检查，并确认失败原因来自名称未导出。
- 验证多个模块消费者能够构建和链接，Asio 运行时及其他需要单一定义的对象没有重复定义或缺失。

当前的 [header_bridge.cpp](../asio/tests/verify/header_bridge.cpp) 与 `contracts_test.cpp` 中的 `header_and_module_share_task_types()` 用于验证跨模式混用。它们不再是目标方案的验收要求，应在实现切换时同步调整；单独的头文件模式测试仍需保留。

## 6. 参考链接

### Vulkan-Hpp 源码

- [旧版模块接口：v1.4.328/vulkan/vulkan.cppm](https://github.com/KhronosGroup/Vulkan-Hpp/blob/v1.4.328/vulkan/vulkan.cppm)：全局包含头文件，再通过 `using` 导出名称。
- [旧版生成器：generateCppmFile()](https://github.com/KhronosGroup/Vulkan-Hpp/blob/v1.4.328/VulkanHppGenerator.cpp#L129)：模块模板及各类导出清单的生成入口。
- [新版模块模板：CppmTemplate.hpp](https://github.com/KhronosGroup/Vulkan-Hpp/blob/2d6804b76eefce6039e6afb8f9da7bba0f499043/generator/snippets/CppmTemplate.hpp)：全局依赖、模块声明、标准库导入与 C++ 头文件的位置关系。
- [新版生成器：generateCppmFile()](https://github.com/KhronosGroup/Vulkan-Hpp/blob/2d6804b76eefce6039e6afb8f9da7bba0f499043/generator/VulkanHppGenerator.cpp#L619)：通过模板生成 `.cppm`，与头文件生成共用 API 数据模型。
- [新版导出宏：vulkan_hpp_macros.hpp](https://github.com/KhronosGroup/Vulkan-Hpp/blob/2d6804b76eefce6039e6afb8f9da7bba0f499043/vulkan/vulkan_hpp_macros.hpp#L121)：`VULKAN_HPP_EXPORT` 的条件定义。
- [新版 C++ 头文件：vulkan.hpp](https://github.com/KhronosGroup/Vulkan-Hpp/blob/2d6804b76eefce6039e6afb8f9da7bba0f499043/vulkan/vulkan.hpp)：模块模式下跳过外部头文件包含，以及条件导出的实际使用。

以上新版源码链接固定到本次核对的提交，便于复核；后续上游可能继续调整。

### 使用与构建文档

- [Vulkan-Hpp：C++20 named module](https://github.com/KhronosGroup/Vulkan-Hpp/blob/2d6804b76eefce6039e6afb8f9da7bba0f499043/docs/Usage.md#c20-named-module)：实验状态、工具链、CMake target、宏配置和消费者示例。
- [Vulkan-Hpp：Building](https://github.com/KhronosGroup/Vulkan-Hpp/blob/2d6804b76eefce6039e6afb8f9da7bba0f499043/docs/Building.md)：生成器及从 XML 重新生成绑定的流程。
- [CMake：C++ modules](https://cmake.org/cmake/help/latest/manual/cmake-cxxmodules.7.html)：模块依赖扫描、`FILE_SET CXX_MODULES` 与工具链支持。
- [Clang：Standard C++ Modules](https://clang.llvm.org/docs/StandardCPlusPlusModules.html)：模块归属、全局模块片段及既有头文件库的模块化方式。

## 当前 core 入口

`<async/core.h>` 及 `core/Awaitable.h`、`core/Executor.h`、`core/Context.h` 属于公开入口。stdexec 的实际类型位于 `execution`，`core` 导出其别名和操作引入；`<async/execution.h>` 及组件头文件提供原生执行能力入口。两组入口在模块模式下导入同一模块，在构建模块自身时由 `LCF_ASYNC_BUILDING_MODULE` 进入声明分支。内部 promise、队列、定时服务和通知注册留在 `execution` 各组件的 `details`，不使用 export。Asio 沿用全局定义与导出清单，补充导出基础类型别名。

跨翻译单元测试分别通过 `execution::Executor` 与 `lcf::async::Executor` 声明同一个函数，验证原生类型与基础别名能够互通。编译失败检查分别覆盖 task 的异常辅助函数、execution 的帧分配辅助函数和内部队列名称；必须因对应名称未导出而失败。
