# Libraries playground

集合入口继续保留在 `agent-workspace/playground/libs`，当前不迁出 playground。async 是其中独立维护的 Git 子模块，拥有自己的开发入口；未来这个集合可以作为 `lcf_cpp_libs` 子模块进入 LCFEngine。集合和 LCFEngine 都可以编写使用各子库能力的 examples。

直接开发 async 时打开 [async](async/README.md)；检验外部接入时打开 [async_consumer](examples/async_consumer/README.md)，选择子模块、FetchContent 或安装包方式。当前目录继续提供集合示例和集合级依赖工具。根 presets 复用 async 的 presets，构建目录随当前打开的根目录确定。

Git 子模块暂用本地仓库验证，URL 指向本机 `build/repositories/async.git`，尚不是可分发的远程配置。有正式地址后使用 `git submodule set-url async <url>` 并将子仓推送到该远程。修改 async 后在子仓提交，再在集合更新指针；集合自身没有自动提交。

原有 async 库历史已从集合历史中提取保留。拆分前的集合历史 bundle、索引和差异备份位于 `build/repository-backup`；子仓文件由子仓管理，集合记录其提交指针。

当前还由外层 LCFEngine 仓库暂存和保存本目录的完整源码快照，便于正式远程仓库就绪前上传开发成果。外层记录普通文件，不记录指向本地仓库的 gitlink；内层 Git 元数据和历史继续留在本机，不随快照上传。克隆 LCFEngine 后即可取得源码并使用默认的源码接入方式，但不会自动获得这两个独立仓库的历史。后续建立正式远程仓库时，再将外层源码快照迁移成子模块引用。

直接在 VS Code 打开 `E:/LCFEngine/agent-workspace/playground/libs`。这里是 CMake、presets、examples 和独立 Git 仓库的入口；async 位于 `async/`。

```text
libs/
├─ CMakeLists.txt
├─ CMakePresets.json
├─ .vscode/
├─ cmake/                  集合级依赖管理脚本
├─ docs/                   项目与模块设计
├─ examples/               使用各库的外部程序
├─ async/
│  ├─ asio/
│  │  └─ tests/             Asio 适配及专属测试
│  ├─ stdexec/
│  │  └─ tests/             stdexec 适配及专属测试
│  └─ tests/                公共接口测试和后端比较基准
└─ build/                  新的构建目录及本地依赖源码缓存
   └─ .gitignore           忽略本目录全部内容
```

旧的 `playground/async` 目录与构建产物已经移除。集合和 async 分别拥有 Git 元数据，可以在各自目录执行 `git status`。

## 选择配置

| preset | 启用内容 |
| --- | --- |
| `dev` | 两个后端的头文件模式；提供 verify、sandbox、benchmark 延迟入口 |
| `ide` | 两个后端的头文件模式；主构建中配置 verify/sandbox，便于逐项发现与调试测试 |
| `asio` | 仅 Asio 的头文件模式及延迟开发入口 |
| `stdexec` | 仅 stdexec 的头文件模式及延迟开发入口 |
| `stdexec-ide` | 仅 stdexec 的头文件模式，主构建测试 |
| `stdexec-msvc-2026` | 仅 stdexec 的 MSVC 2026 模块模式，主构建测试 |
| `stdexec-benchmark` | 仅 stdexec benchmark |
| `compare` | 两个独立程序运行同一批后端比较负载 |

`dev` 提供延迟测试目标，配置时不解析 GoogleTest 或 Google Benchmark。`ide` 为直接运行和调试用例而立即配置 GoogleTest，benchmark 关闭。stdexec 本身的依赖解析仍遵守库的来源策略；preset 选择 `FETCH`，支持本地源码覆盖。

现有 `msvc-2026`、`clang`、`gcc` 配置保持 Asio 单后端用法。两个后端的公开命名空间和模块名相同，分别链接到各自的示例/测试程序。

## 项目大纲

两个库都登记了自身头文件，头文件模式下也可以在 CMake Tools 项目大纲中浏览。延迟测试 utility target 同样登记对应源码，使打开项目后就能导航到用例。

大纲按 CMake `FOLDER` 属性分组：

```text
async/
├─ asio/
│  ├─ async_asio
│  ├─ dependencies/
│  └─ tests/
│     ├─ verify/
│     ├─ sandbox/
│     └─ benchmark/
├─ stdexec/
│  ├─ async_stdexec
│  └─ tests/
│     ├─ verify/
│     ├─ sandbox/
│     └─ benchmark/
└─ tests/
   └─ benchmark/           compare 配置下的公共比较入口
examples/
└─ async/
   ├─ asio/
   └─ stdexec/
top_dependencies/
```

只会出现当前 preset 启用的分支；例如 `ide` 不提供 benchmark，`stdexec` 不提供 Asio。各后端执行同一份公共接口测试，公共源码位于 `async/tests/verify`，在测试目标的 `interface` 源码组中可见。

头文件库在大纲中是可浏览的 INTERFACE target，编译与运行检查由 sandbox 承担。模块模式下对应库是可编译的静态库。

## 构建与运行

在当前目录、相应的编译器环境中：

```powershell
cmake --preset dev
cmake --build --preset debug --target async_example async_stdexec_example
cmake --build --preset debug --target async_sandbox async_stdexec_sandbox
cmake --build --preset debug --target async_verify async_stdexec_verify

cmake --preset ide
cmake --build --preset ide-debug --target async_verify async_stdexec_verify

cmake --preset compare
cmake --build --preset compare-release
```

构建 sandbox/verify 目标会执行相应测试；构建 example 只编译程序，运行和调试时在 CMake Tools 选择对应 example。VS Code 任务同样按 Asio/stdexec 区分，并已移除旧 compile 任务。

顶层依赖管理目标仍为 `top_collect_deps`、`top_list_dependency_versions`、`top_sync_deps` 和 `top_update_deps_to_latest`，分组为 `top_dependencies`。子库自己的依赖脚本不需要知道集合级清单，已有版本选择得到保留。

## 构建目录的忽略规则

使用构建目录自身的 `.gitignore`：

```gitignore
*
```

CMake 配置独立构建目录时会在其中创建此文件，已有文件保留。顶层 `.gitignore` 不列举 build 文件夹；删除并重建 build 后，配置阶段会重新生成忽略规则。当前保留的第三方源码缓存也位于被忽略的 build 中。

## 进一步阅读

- [stdexec 使用、调度预算与实现](async/stdexec/README.md)
- [公共测试与后端比较基准](async/tests/README.md)
- [CMake 项目与依赖设计](docs/cmake-project-design.md)
- [头文件与 modules 的兼容方案](docs/cpp-modules-transition.md)
