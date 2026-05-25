# Shader Manifest — Ninja 风格的增量编译判定层

> 阅读这份文档前，假设你已经知道 `libs/shader_core/` 大致干什么（slang/glsl → SPIR-V）。
> 这份文档只解释一件事：**为什么 shader 在没有变化时能跳过 slang 编译，以及它是怎么做到的。**

## 1. 这个东西解决了什么问题

引入 manifest 之前的 cache 路径（看历史可查 `217e9aae`）是这样的：

```
compileSlangSourceToSpv(file_path)
    ↓
slang.loadModuleFromSourceString          ← 已经付出了 slang frontend 的代价
    ↓
slang.getDependencyFilePath() × N         ← 读取所有依赖文件
    ↓
hash(source + deps + config) → cache_hash
    ↓
ShaderCache.tryLoad(cache_hash)?
    ├── HIT  → 返回                       ← 但 slang frontend 已经白跑了
    └── MISS → 真正编译 → ShaderCache.store
```

问题：**判断"要不要编译"的代价已经接近"半次编译"**。要算 hash 必须先让 slang 解析依赖，等于每次启动都付一次 slang frontend 的钱，无论命中与否。

我们想要的目标是 ninja / make 那种判定方式：**只 stat 几个文件**，不读内容、不调编译器，就能决定"用不用重编"。

## 2. 心智模型：两层 cache

不要把 manifest 想成"另一个 cache"，它和原 ShaderCache **不在同一层**：

```
┌──────────────────────────────────────────────────────────────┐
│  Manifest = 路径索引（轻）                                     │
│    path → { fingerprint of source + deps, compile_input_hash } │
│    回答："需不需要重编？"                                       │
└──────────────────────────────────────────────────────────────┘
                          │ 如果回答"不需要"
                          ▼
┌──────────────────────────────────────────────────────────────┐
│  ShaderCache = 内容寻址产物（重）                              │
│    compile_input_hash → SPIR-V .spvbin                       │
│    回答："给我这个 hash 对应的产物。"                           │
└──────────────────────────────────────────────────────────────┘
```

两层职责单一：

- Manifest 不存 SPIR-V，只存"路径 + 指纹 + 一个 hash"。整个 manifest 通常 < 1 MB。
- ShaderCache 不知道路径，只是 `hash → bytes` 的字典。

ninja 的同等结构对照：

| 我们 | ninja |
| --- | --- |
| `manifest.bin` | `.ninja_deps` + `.ninja_log` |
| `ManifestEntry.main_fp` | `Node::mtime` |
| `ManifestEntry.deps[]` | `DepsLog` 里的 dependency edges |
| `ProductRef.compile_input_hash` | ninja **没有**这一层（它只判定要不要重跑命令，不做内容寻址 cache） |
| `ShaderCache.tryLoad` | 没有对应物 |

注意第三/第四行：ninja 命中后直接"跳过命令"，不需要"读取产物"——因为产物就在它原本生成的位置。我们多了 `compile_input_hash → spvbin` 这层，是因为 SPIR-V 是我们自己管理的产物，不像 ninja 让用户的 `cl.exe` 自己写 obj 到固定路径。

## 3. 命中判定流程（最核心的流程图）

下面这张图就是 `compileSlangSourceToSpv(file_path)` 的全部逻辑骨架，对应 `libs/shader_core/src/ShaderCompiler.cpp:281-356`：

```
入参：file_path（用户传的，可能是 "test_shaders://shader_a.slang"）
   │
   ▼
Config::resolvePath（解析虚拟路径前缀）+ weakly_canonical
   │
   ▼ canonical 绝对路径作为 manifest key
Manifest::find(canonical)
   ├── nullptr → MISS
   └── ManifestEntry *entry
       │
       ▼ 同一个 entry 上四道闸
       (1) entry.target_profile / compiler_options 与当前 slang::Config 一致？
       (2) Manifest::fingerprintMatches(canonical, entry.main_fp)？
       (3) 对 entry.deps 中每一个 dep：fingerprintMatches(dep_path, dep_fp)？
       (4) ShaderCache::tryLoad(entry.products[0].compile_input_hash) 返回非 nullopt？
       │
       任一 false → MISS
       全部 true   → HIT，直接返回 UnitList
```

**HIT 时：完全不调 slang。** 代价就是 stat 几次 + 读一个 .spvbin 文件。这是 manifest 存在的全部理由。

**MISS 时：走原本的完整 slang 路径**（在同一个函数末段，`compileSlangCore` 调用），编译完后做两件事——`ShaderCache.store(cache_hash, units)` 写产物，`manifest.upsert(new_entry)` 写索引。

## 4. 指纹的两段式实现（重要细节）

`Manifest::fingerprintMatches` 在 `libs/shader_core/src/Manifest.cpp:152-179` 的核心是：

```
快路径：mtime + size 都等于 expected → 直接 true，零文件读
慢路径：mtime/size 不等 → 读文件、算 content_hash、与 expected.content_hash 比
```

为什么不只用 mtime？因为 `git checkout` 切分支会把所有文件 mtime 改掉，但内容可能未变；只看 mtime 会假阳性触发整轮重编。content_hash 兜底救这种情况。

为什么不只用 content_hash？因为对 N 个依赖每个都读一遍文件再 hash，启动会变慢。stat 一次几乎免费。

ninja 的取舍是另一种：**只用 mtime**——它假设你不会乱切分支。我们做了更保守的选择。

## 5. 二进制格式（理解持久化）

`libs/shader_core/src/Manifest.cpp:14-17` + `loadFromDisk/writeToDisk` 实现的格式：

```
header
  magic            : u32  = 0x4C434D46 ("LCMF")
  version          : u32  = 1
  slang_version    : lstring                ← spGetBuildTagString，整 manifest 级别的失效闸
  entry_count      : u32

entry[entry_count]
  entry_size       : u32                    ← 整条 entry 字节数（不含本字段），关键
  source_path      : lstring
  main_fp          : { mtime:u64 size:u64 content_hash:u64 }
  dep_count        : u32
  deps[dep_count]  : { path:lstring mtime:u64 size:u64 content_hash:u64 }
  target_profile   : u8
  compiler_options : u32
  product_count    : u32                    ← 第一版恒为 1（变体扩展槽）
  products[]       : { variant_key:lstring compile_input_hash:u64 }
                                            ← 第一版 variant_key 长度恒为 0
lstring = { len:u32, bytes:[u8] }
```

两个最重要的格式约束：

**(a) `entry_size` 块前缀** 让未来加字段不破坏旧 reader——旧版本读到不认识的 entry，可以根据 `entry_size` 直接跳到下一条 entry，而不是放弃整个 manifest。这是 ninja `.ninja_deps` 用的同款思路（每条记录前有长度前缀）。

**(b) `slang_version` 在全局头**，意味着 slang 升级（`spGetBuildTagString` 变了）→ 整个 manifest 作废，因为 SPIR-V 字节级布局可能跟着 slang 版本变。
而 `target_profile` / `compiler_options` 在每条 entry 里，因为用户可能针对单个 shader 临时调 profile，不应让其他 shader 全部重编。

## 6. 单例 + 内存 dirty + batch flush

`Manifest` 是单例（`libs/shader_core/include/shader_core/Manifest.h:74-110`）：

- 启动时 lazy load——首次 `find()` 或 `upsert()` 才真正读 `manifest.bin`，未编译任何 shader 的程序根本不会触发 IO（`Manifest::ensureLoaded`，`Manifest.cpp:181-186`）。
- `upsert()` 只改内存 + 设 `m_dirty = true`，不写盘。
- 析构时如果 `m_dirty` 真才 `writeToDisk()`（`Manifest.cpp:97-104`）。
- 暴露 `Config::flushShaderManifest()`（`config.cpp` 末尾）转发到 `Manifest::flush()`，让外部能在需要时强制落盘——例如测试脚本要模拟"跨进程边界"，或者未来热重载场景。

ninja 的对应做法是：每次 build 完写一次 `.ninja_log`，崩溃丢失的话下次 startup 全量重判一次，正确性不丢。我们一样——崩溃丢的是新 entry，下次启动会 MISS 一次重新写入。

## 7. 一次"修改 common.slang"的端到端追踪

为了把流程串起来，假设你：

1. 上次跑过测试，manifest 里已经有 `shader_a/b/c.slang` 三条 entry，每条 deps 列表里都有 `common.slang` 的指纹。
2. 你手动改了 `common.slang`（无论是改空格还是改函数）。
3. 重新跑 `shader_core_test`。

发生的事：

```
compileSlangSourceToSpv("test_shaders://shader_a.slang")
  ├── canonical = ".../shader_a.slang"
  ├── manifest.find → 找到 entry_a
  ├── 闸 1 通过
  ├── 闸 2: fingerprintMatches(shader_a.slang, entry_a.main_fp)
  │       ├── stat: size 没变, mtime 没变 → 快路径 true
  │       └── 通过
  ├── 闸 3: 对 entry_a.deps[0] = (common.slang, old_fp)
  │       ├── stat: size 变了（你 append 了空格也算变）
  │       │   或 mtime 变了
  │       ├── 慢路径：读 common.slang 内容 → content_hash
  │       ├── 与 old_fp.content_hash 比 → **不等**
  │       └── 返回 false
  └── 走 MISS 路径：完整 slang 编译 + manifest.upsert（写入新的 dep fingerprint）
```

shader_b/c 同理，三个都会 MISS——这就是"级联失效"的实现：依赖图本身没存在 manifest 里，但每条 entry 自带"我依赖谁、依赖时是什么状态"的快照，三个 entry 各自检查后**各自得出 MISS 结论**。这意味着 manifest 不需要反向索引（"谁依赖了 common"）也能工作。这一点和 ninja 的 `DepsLog` 一致：每条 record 是"target → list of inputs at last build"，扇入扇出是查询时计算出来的。

## 8. 第一版刻意没做的事

留在源码注释和 plan 里的"future work"，理解它们能避免你之后误以为是 bug：

- **没有 `.spvbin` GC**：MISS 一次就生成一个新的 `<hash>.spvbin`；老的 hash 对应的 spvbin **不会被清理**。如果测试脚本 append 含时间戳的内容，每次跑产物 hash 都不同，`.shader_cache/` 会持续增大。这不是 manifest bug，是 plan 里 "Q6 不引入 GC" 的直接后果。
- **没有 token-level normalization**：改空格、调格式都会触发重编。前文 §4 说过"ninja 只用 mtime"，我们多了 content_hash 兜底，但 content_hash 看的是字节内容——不会忽略空白。
- **没有跨进程并发协调**：单例 manifest 假设单进程访问。两个进程同时编译写同一个 `manifest.bin` 会互相覆盖（plan Q6 的"工业引擎演进"路径里讨论过；当前规模下不需要）。
- **没有变体支持**：`product_count` 恒为 1，`variant_key` 长度恒为 0。格式上槽位预留好了，添加变体不会破 manifest 兼容性，但实际 dispatch 还没接通。

## 9. 如果你以后要加 efsw 热重载，从哪里下手

efsw 是后续工作，但 manifest 已经为它留好了出口：

- **要监听哪些路径**：枚举 `Manifest::instance().m_entries`（需要给一个 `forEachEntry` 接口或迭代器），把所有 `entry.source_path` 和 `entry.deps[].path` 收集去重，就是 watch 列表。
- **某文件变了之后怎么办**：在内存中把所有 `entry.deps` 里包含该路径的 entry 标记为"待重编"。最简单的实现：把它们从 `m_entries` 里 `erase` 掉——下次 `compileSlangSourceToSpv` 自动 MISS 走完整流程。
- **反向索引**：plan 里说不持久化，按需在内存中 O(N) 重建。efsw 启动时扫一遍 `m_entries` 建个 `unordered_map<path, vector<source_path>>` 即可。

这些都不需要改 manifest 的二进制格式。

## 10. 想更深一步看代码，建议的阅读顺序

如果你只看一遍：

1. `libs/shader_core/include/shader_core/Manifest.h`（170 行不到，含详细注释，是入口）
2. `libs/shader_core/src/ShaderCompiler.cpp:281-356`（`compileSlangSourceToSpv(file_path)`，看快路径判定 + MISS 时怎么 upsert）
3. `libs/shader_core/src/Manifest.cpp:131-179`（`computeFingerprint` + `fingerprintMatches`，指纹两段式）

如果还想看持久化怎么做：

4. `libs/shader_core/src/Manifest.cpp:188-300`（`loadFromDisk`，含 `entry_size` 跳过逻辑）
5. `libs/shader_core/src/Manifest.cpp:303-356`（`writeToDisk`）
6. `libs/shader_core/include/shader_core/buffer_io.h`（共享的 `BufferReader/BufferWriter`，含 `reserveSlot/patch` 用于回填 `entry_size`）

Manifest 的工程量很小（实现 ~350 行 + 头 ~120 行），但每一段都对应了 plan 里一个明确的设计选择——选项被砍掉的那一面在 `agent-notes/` 已经被删除的旧 plan 里讨论过，看这份文档时不需要重新再选一遍。
