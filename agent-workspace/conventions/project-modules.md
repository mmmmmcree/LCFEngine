# Project Module Index — LCFEngine

Living index of existing project modules and tools the agent must reuse instead of reinventing. **Grown incrementally**: an entry is added when the human points at a module ("xxx 需要参考/使用 xxx"), not by bulk scanning. Read before writing code alongside [`cpp-style.md`](./cpp-style.md).

| Module / tool | What it provides | Use it for |
| --- | --- | --- |
| `utilities/include/enums/enum_count.h` | `lcf::enum_count_v<Enum>` (magic_enum-backed, requires `enum_c` concept) | sizing arrays indexed by enum — never hand-write an `eCount` sentinel |
| `containers/` (robin-map wrapper) | `tsl::robin_map` integration | hash maps with sparse/dynamic keys; dense small enums use `std::array` + `enum_count_v` instead |
| `cmake/scripts/deps.cmake` | per-module build-dependency entry point | the mental model mirrored by vkc's `<module>/feature_dependencies.h` |
| `utilities/include/type_traits/member_pointer_traits.h` | `lcf::member_pointer_traits<M>::class_type`, `member_pointer_class_t` | extracting the owning struct from a pointer-to-member NTTP |
