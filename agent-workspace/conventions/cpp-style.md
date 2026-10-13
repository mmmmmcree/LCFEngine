# C++ Style

## Naming

- Use PascalCase for ordinary classes, structs, enums, and type aliases; camelCase for non-static member functions; snake_case for static member functions, free functions, parameters, and local variables.
- Use `m_snake_case` for data members and `k_snake_case` for constexpr objects and file-scope constants. Ordinary local `const` variables retain snake_case.
- Use `ePascalCase` for ordinary enum values.
- Follow standard-library naming for traits and related template utilities: `enum_traits`, `value_type`, `value`, `*_t`, `*_v`, and snake_case query functions. Use `snake_case_c` for concepts.
- Mark variables with different access semantics using these suffixes: raw pointer `_p`, unique_ptr `_up`, shared_ptr `_sp`, weak_ptr `_wp`, optional `_opt`, ResourceHandle `_rh`, iterator `_it`. Combine with `m_` for members.
- Name configuration objects after what they configure and retain `_info`, such as `image_pass_info`. Avoid generic `info` when it obscures the role.
- Use full words for folders and ordinary types. Reuse established namespace abbreviations such as `vkc`, `bs`, and `dsp`; ask before introducing new abbreviations.
- Avoid C++ keywords as identifiers, including contextual keywords such as `module`.
- Avoid repeating the enclosing namespace in a name: use `manifest::Entry`, not `manifest::ManifestEntry`.
- Name class files after the class (`DeviceContext.h`, `DeviceContext.cpp`); use snake_case for free-function and collection files (`create_device.h`, `info_structs.h`).

## Formatting

- Indent with 4 spaces. Do not indent namespace contents.
- Put opening braces on a new line for nonempty types and multiline functions; keep them on the same line for namespaces, control statements, try/catch, and lambdas. Keep empty type bodies on the declaration line: `struct trait : base {};`.
- Always brace control-statement bodies. Simple getters, setters, and single-statement branches may stay on one line: `if (not valid) { return error; }`.
- Do not impose a line-length limit. Break at meaningful structural boundaries.
- When splitting a parameter list, indent continuation lines by 4 spaces and keep the closing parenthesis with the last parameter. For multiline constructor initializer lists, put `:` at the end of the signature and indent the initializers.
- Align continued chained calls at one indentation level, with each subsequent `.method()` on its own line.
- Write pointers and references as `Type * value_p`, `const Type & value`, and `Self && other`.
- Leave a space between `template` and `<`: `template <typename T>`, `template <>`.
- Keep access labels aligned with the class braces, and indent declarations beneath them.
- Do not insert blank lines between consecutive member declarations or definitions. Separate class responsibility groups with access labels rather than blank lines.
- Use `not`, `and`, and `or` for logical operators.

## Class Organization

- Apply these organization rules to structs with behavior as well as classes. Data-only aggregate structs may stay simple.
- Put private type aliases near the top of the class. Use `using Self = ClassName;`; use `Base` and `Root` when naming a base class or wrapped structure.
- Define class-owned container types through private, semantic `using` aliases. Use those aliases consistently in members and related interfaces so the container implementation can be changed in one place.
- Divide the class into responsibility groups using access labels, repeating a label even when adjacent groups have the same access.
- Group destruction, construction, copying, moving, assignment, and conversions separately from the operational interface, using separate `public:` sections.
- Put private helper methods, including static methods, in their own `private:` section, separate from type definitions, aliases, and data members. Repeat `private:` even when access is already private.
- Explicitly qualify calls to methods on the current object with `this->`. Access data members directly through their `m_` names.
- Prefer file-local helpers over private methods for implementation work that does not require object state.
- Allow copying only when it has meaningful semantics; declare move operations for types intended to be movable.
- Expose singletons through a static `instance()` method with a private constructor.

```cpp
class RequestBatch
{
    using Self = RequestBatch;
    using RequestList = std::vector<QueueRequest>;
public:
    ~RequestBatch() noexcept = default;
    RequestBatch() noexcept = default;
    RequestBatch(const Self &) = delete;
    RequestBatch(Self &&) noexcept = default;
    Self & operator=(const Self &) = delete;
    Self & operator=(Self &&) noexcept = default;
public:
    const RequestList & getRequests() const noexcept { return m_requests; }
private:
    void clearRequests() noexcept { m_requests.clear(); }
private:
    RequestList m_requests;
};
```

## Source Layout

- Use `#pragma once` in headers. In a .cpp file, include its corresponding header first.
- Keep .cpp files readable from the interface down to implementation details:
  1. Includes and namespace aliases.
  2. File-scope anonymous namespace with helper declarations.
  3. Named namespace with core interface definitions.
  4. File-scope anonymous namespace with helper definitions.
- Keep anonymous namespaces outside the project's named namespaces.

## Control Flow

- Keep the main path flat. Check preconditions and errors early, then return immediately.
- Filter loop iterations with `continue` instead of wrapping the main work in another conditional.
- Do not wrap the remaining path in `else` after a branch that already exits.

## Comments

- Code explains itself. Prefer clear names and structure over explanatory comments.
