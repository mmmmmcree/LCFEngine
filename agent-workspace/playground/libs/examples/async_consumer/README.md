# async 的三种接入方式

这是独立的消费项目，可以直接在 VS Code 打开当前目录。三种方式都编译同一个 [main.cpp](main.cpp)，业务代码始终链接 `lcf::async_asio` 或 `lcf::async_stdexec`，运行结果为 `async result = 42`。

| configure preset | 如何得到 async |
| --- | --- |
| `submodule-asio` / `submodule-stdexec` | 对集合里的 `async/` 子模块执行 `add_subdirectory()` |
| `fetch-asio` / `fetch-stdexec` | FetchContent 从 Git 仓库检出固定提交 |
| `package-asio` / `package-stdexec` | `find_package(LCFAsync CONFIG REQUIRED COMPONENTS ...)` |

源码来源与 async 自己的依赖来源是两件事。示例源码模式选择 `LCF_ASYNC_DEPENDENCY_PROVIDER=FETCH`；可以改为 `PACKAGE` 或 `PROVIDED`。安装包模式始终查找公共依赖包。

## 子模块或本地源码

```powershell
cmake --preset submodule-asio
cmake --build --preset submodule-asio-debug
```

`LCF_ASYNC_SOURCE_DIR` 默认指向 `../../async`，也可以指定另一份本地 checkout。子模块的初始化属于 Git；CMake 不自动执行 `git submodule update`。

## FetchContent

```powershell
cmake --preset fetch-asio -DLCF_ASYNC_GIT_REPOSITORY=path/to/async.git -DLCF_ASYNC_GIT_TAG=commit_hash
cmake --build --preset fetch-asio-debug
```

本地验证仓库在集合的 `build/repositories/async.git`。这里使用固定提交，而不是浮动的 `main`。修改子仓后，先提交并推送到本地 bare 仓库，再更新 `LCF_ASYNC_GIT_TAG`。

## 安装包

先在 async 项目开启安装，构建并安装；然后配置消费项目：

```powershell
cmake --preset package-asio -DCMAKE_PREFIX_PATH=path/to/installed/prefix
cmake --build --preset package-asio-debug
```

Asio 的 `asio::asio` 或 stdexec 的 `STDEXEC::stdexec` 也必须由依赖包提供；可以通过同一个 `CMAKE_PREFIX_PATH` 列表或工具链指定。async 不附带这两个第三方库的安装文件。缺少所选组件或依赖包时会在配置阶段报错，不回退下载。

## 本机验证配置

`CMakeUserPresets.json` 保存本机忽略的 `local-*` presets，包含当前缓存路径、本地 bare 仓库固定提交，以及移动后的测试安装前缀。它们用于立即在 CMake Tools 中选择三种方式；常规 presets 不绑定本机路径。

其中 `build/dependency-packages` 是验证脚本为现有真实第三方源码缓存创建的薄 CMake 包适配，用于检验安装包依赖边界。实际项目应替换成 vcpkg 等来源提供的依赖包。移动 async 的安装前缀后仍能构建消费程序，是本例验证的可迁移性边界。
