# vk_core Descriptor Set Design

Status: design baseline, implementation validation pending.

This document records the descriptor-set design agreed for `vk_core`. The
first implementation is expected to expose details that may require changes;
the invariants in this document are the intended starting contract.

## 1. Goals

The descriptor module has two independent responsibilities:

1. Hide descriptor allocation policy from callers.
2. Hide frame-in-flight replication and update policy from callers.

Callers should modify one logical `DescriptorSetProxy`. They should not need
to know whether the device uses descriptor pools, descriptor buffers, or
descriptor heaps, nor how many physical copies are currently required.

The preferred backend order is:

```text
DescriptorHeap > DescriptorBuffer > DescriptorPool
```

This order is a performance policy, not a correctness assumption. A backend
is selected only when its device features, extensions, resource types, and
pipeline requirements are all available.

## 2. Non-goals and module boundary

`vk_core` owns Vulkan descriptor mechanisms and their resource lifetime
contracts. It does not own render-layer policy such as a fixed number of
frames in flight or a swapchain image-ring policy.

The descriptor module may manage the physical backing required by a proxy,
including dynamically growing backing slots. The caller supplies the command
recording context through `CommandBufferProxy`.

The module must not require callers to use a raw `vk::DescriptorSet` as the
universal representation. Descriptor pools, buffers, and heaps have different
binding models.

## 3. Build-time backend selection

Backend selection follows the existing `wsi::probed::Swapchain` pattern. The
probe exposes one capability with ordered options:

```text
descriptor heap header
descriptor buffer header
descriptor pool header
```

The application requests the capability in its probe configuration:

```cpp
capabilities.require<descriptor_set::DescriptorCapability>();
```

The build-time probe tests the options in priority order, registers the
requirements of each candidate, and generates a configuration header that
includes the selected option. The selected header defines the concrete
`descriptor_set::probed` aliases.

The intended public shape is:

```cpp
namespace lcf::vkc::descriptor_set::probed {

using Backend = detail::HeapBackend; // selected by the generated header
using DescriptorSetLayout = detail::HeapDescriptorSetLayout;
using DescriptorSetProxy = detail::HeapDescriptorSetProxy;

} // namespace lcf::vkc::descriptor_set::probed
```

The pool and buffer headers provide the same aliases with different concrete
types. The final application binary therefore contains one selected
implementation. There is no runtime backend variant, virtual dispatch, or
backend branch in the proxy hot path.

The probe executable and its capability-selection loops may use ordinary
runtime control flow. That logic is build-time tooling, not descriptor use at
runtime.

## 4. Static backend policy

Each backend has a stateless policy/configuration type. The type is used while
registering device requirements and building backend-dependent Vulkan objects:

```cpp
struct BackendPolicy {
    static void register_requirements(
        InstanceExtensionManifest &,
        DeviceExtensionManifest &) noexcept;

    static void configure_device_context(DeviceContextCreateInfo &) noexcept;
    static void configure_shader_program(ShaderProgramInfo &) noexcept;
    static void configure_graphics_pipeline(GraphicsPipelineInfo &) noexcept;
};
```

The exact function set may be split as the implementation develops, but the
principle is fixed: backend-dependent setup is expressed through a concrete
type selected at build time.

This is required because descriptor heaps are not merely a faster descriptor
set allocator. They change pipeline and shader creation requirements:

- descriptor buffers require descriptor-buffer pipeline support and use
  descriptor-buffer offsets;
- descriptor heaps require descriptor-heap pipeline/shader support and shader
  descriptor mapping;
- ordinary descriptor pools use regular descriptor set layouts and pipeline
  layouts.

The common logical descriptor layout must therefore be materialized by the
selected backend policy. A raw `vk::DescriptorSetLayout` is not a sufficient
cross-backend layout abstraction, especially for descriptor heaps.

## 5. Logical descriptor API

The caller-facing API uses vkc-owned logical descriptor values rather than
backend-specific Vulkan write structures. Representative values are:

```cpp
struct BufferDescriptor {
    Buffer buffer;
    vk::DeviceSize offset = 0;
    vk::DeviceSize range = vk::WholeSize;
};

struct ImageDescriptor {
    ImageView view;
    vk::ImageLayout layout;
};

struct SamplerDescriptor {
    Sampler sampler;
};

struct CombinedImageSamplerDescriptor {
    ImageView view;
    Sampler sampler;
    vk::ImageLayout layout;
};
```

The exact resource wrapper names may change with the memory/image modules. The
important property is that the values contain enough resource identity and
metadata for every backend to materialize a descriptor.

All concrete proxy types expose the same logical setters:

```cpp
DescriptorSetProxy & setBuffer(
    uint32_t binding,
    uint32_t array_index,
    BufferDescriptor value);

DescriptorSetProxy & setImage(
    uint32_t binding,
    uint32_t array_index,
    ImageDescriptor value);

DescriptorSetProxy & setSampler(
    uint32_t binding,
    uint32_t array_index,
    SamplerDescriptor value);

DescriptorSetProxy & setCombinedImageSampler(
    uint32_t binding,
    uint32_t array_index,
    CombinedImageSamplerDescriptor value);
```

Convenience overloads may omit `array_index` for scalar bindings. Binding
validation must use the Vulkan binding number, not the position of a binding in
an implementation vector.

The logical API can grow to cover texel buffers, acceleration structures,
inline uniform blocks, and other descriptor types without exposing the
materialization format of a particular backend.

## 6. Authority and versions

The proxy contains an authority state. The authority is the only state the
caller directly mutates. It stores the latest logical descriptor values and a
monotonically increasing version.

Each physical backing slot stores at least:

```cpp
struct BackingSlot {
    uint64_t materialized_version = 0;
    Backing backing;
    SlotState state; // available, recording, in_flight, retired
    RetireToken retire_token;
};
```

An authority update changes the logical version and marks the affected
bindings dirty. A slot that already materialized the current version is a
no-op when committed again.

The authority must retain enough resource ownership or lease information to
prevent a descriptor from referring to a resource that has already been
destroyed. A committed slot pins the exact resources used by that materialized
version until the command using it is no longer in flight.

## 7. Dynamic physical backing

The proxy does not require a fixed frame-copy count. Physical backing grows on
demand:

1. `commitUpdate(cmd)` looks for a slot that is available or whose retire
   token has completed.
2. If a reusable slot exists, it is updated in place.
3. If all suitable slots are occupied and this command needs another version,
   the allocator creates a new backing slot.
4. The new slot is materialized from the authority state and attached to the
   command.
5. Old slots are retired and recycled only after their GPU use has completed.

`ResourceLease` is used to pin resources and backing objects into the command
batch. A lease alone is not the complete slot-availability test: slot state
and the queue/timeline retire token are also required to determine whether a
backing can be overwritten.

The implementation may use pools, generations, or append-only chunks for
backing storage. Descriptor heap implementations should avoid overwriting a
heap range while any command buffer can still read it. Reallocation must keep
the old heap memory alive until those command buffers have been freed or
reset, and until the associated GPU work is complete.

## 8. `commitUpdate` contract

The required common update entry point is:

```cpp
std::error_code commitUpdate(
    CommandBufferProxy & cmd) noexcept;
```

The command buffer is the synchronization boundary from the caller's point of
view. A normal usage sequence is:

```cpp
proxy.setBuffer(0, 0, buffer_value)
     .setImage(1, 0, image_value);

proxy.commitUpdate(cmd);
proxy.bind(cmd, 0);
```

The operation is synchronous as an API action: after `commitUpdate` returns,
the required update commands have been recorded and the proxy has selected a
backing for this command. For buffer and heap backends, descriptor bytes may
still be copied by the GPU when the command is submitted.

Backend behavior:

### Descriptor pool

The backend allocates a `vk::DescriptorSet` from an internal pool and records
or performs `vkUpdateDescriptorSets`. Pool updates are host/API operations;
the resulting descriptor set is bound with the regular descriptor-set path.

### Descriptor buffer

The backend obtains layout size and binding offsets, encodes descriptors into
descriptor-buffer-compatible memory, and records any required staging copy.
Device writes are synchronized for subsequent shader access with
`VK_ACCESS_2_DESCRIPTOR_BUFFER_READ_BIT_EXT`.

Host-written descriptor data receives the normal host-write visibility at
queue submission; device-written descriptor data requires the explicit
descriptor-buffer read synchronization.

### Descriptor heap

The backend maintains resource and sampler heap ranges. It obtains descriptor
bytes through the heap descriptor-writing commands and records their transfer
to device memory. Resource and sampler heap reads use:

- `VK_ACCESS_2_RESOURCE_HEAP_READ_BIT_EXT`
- `VK_ACCESS_2_SAMPLER_HEAP_READ_BIT_EXT`

Heap reserved ranges must remain untouched by the application while they are
bound as reserved ranges in live command buffers. Heap state binding also has
the Vulkan-specified interaction with descriptor-set and descriptor-buffer
state, so binding order is part of the backend's command-recording policy.

## 9. Common binding API

Although `commitUpdate` is the central contract, a useful complete abstraction
also needs a common binding operation:

```cpp
std::error_code bind(
    CommandBufferProxy & cmd,
    uint32_t set_index) const noexcept;
```

The concrete implementation maps this to:

- `vkCmdBindDescriptorSets` for pool;
- descriptor-buffer binding and set-offset commands for buffer;
- resource/sampler heap binding and backend-specific mapping state for heap.

The public proxy should not expose a universal `getHandle()` because the
underlying object may be a descriptor set, a buffer address/offset, or two heap
addresses.

If the command layer later provides a higher-level helper, the desired caller
surface is:

```cpp
cmd.bindDescriptorSet(proxy, set_index);
```

That helper may call `commitUpdate` and `bind` in the correct order, while the
two operations remain separately available for explicit recording control.

## 10. Pipeline and shader integration

The logical descriptor layout is shared by all backends, but its Vulkan
materialization is not.

- Pool and buffer backends can materialize descriptor set layouts and use a
  pipeline layout containing those layouts.
- Heap backend materializes shader descriptor set/binding mappings and uses the
  descriptor-heap pipeline and shader creation flags. It is not required to
  produce an ordinary descriptor set layout as its binding object.

The selected `Backend` policy is therefore also the compile-time bridge for
pipeline and shader construction. Pipeline creation must use the same selected
backend as the proxy; mixing a heap proxy with a pool-style pipeline is an
invalid configuration.

## 11. Capability and fallback rules

The default selection order is heap, then buffer, then pool. An option is
eligible only if all of its requirements are satisfied, including:

- required instance/device extensions and features;
- descriptor types needed by the logical layout;
- buffer usage and device-address requirements;
- pipeline and shader creation requirements;
- memory and alignment limits.

The generated `probed` type is intentionally a build-time choice. A binary
built with the selected heap header is not expected to become a different
backend at runtime. A runtime fallback, if required by a future product
configuration, should be a separate explicit compatibility layer rather than
reintroducing dynamic dispatch into `DescriptorSetProxy`.

## 12. Invariants

The implementation must preserve these invariants:

1. A proxy mutation changes authority state, not an arbitrary physical slot.
2. Every materialized slot identifies the authority version it contains.
3. A slot is never overwritten while a live command can read it.
4. A committed command pins the backing and referenced resources it uses.
5. Repeating `commitUpdate` for an unchanged version is cheap and does not
   duplicate writes unnecessarily.
6. All three backends expose the same logical setter and commit contracts.
7. Backend-specific Vulkan handles and binding commands do not leak through
   the common proxy interface.
8. Pipeline and shader creation use the same backend policy as descriptor
   materialization.

## 13. Implementation order

The implementation should proceed in increasing backend complexity:

1. Define logical layout, descriptor values, authority/version tracking, and
   the common proxy contract.
2. Implement the pool backend as the behavioral reference.
3. Add dynamic slot reuse, lease pinning, and timeline-based retirement.
4. Implement descriptor-buffer materialization and command recording.
5. Implement descriptor-heap resource/sampler ranges and synchronization.
6. Add the capability probe options and generated `probed` aliases.
7. Integrate backend policy into pipeline and shader construction.
8. Validate the same example source against the generated pool, buffer, and
   heap configurations where hardware support permits.

The first implementation should prefer clear validation and lifetime checks
over premature allocator tuning. The allocator layout, dirty-binding strategy,
and chunk sizing are expected to be refined by profiling and validation-layer
feedback.

## 14. Expected practice-discovered questions

The following are deliberately left open until implementation exposes real
constraints:

- whether one command may reuse a proxy backing without an explicit command
  recording identity;
- the most useful public representation for image-view and sampler creation
  metadata;
- whether staging copies should be recorded inline or delegated to a transfer
  queue;
- how descriptor heap chunks should reserve implementation ranges while
  maximizing reuse;
- whether dirty tracking should operate per binding, per array element, or per
  complete materialization generation;
- which descriptor types can share common setter overloads without making the
  logical API ambiguous.

