## Rendering model

Batched indirect rendering. All geometry of a given vertex format shares one vertex/index buffer; per-instance and per-material data live in storage buffers indexed from the shader. Two vkCmdDrawIndexedIndirect calls per frame draw the whole scene (static and skinned). No per-draw CPU work, no per-draw descriptor rebinding.

2-pass compute skinning: one pass samples animation tracks, the second resolves the bone hierarchy.

# VHL

Stands for Vulkan Hardware Layer, a thin Vulkan abstraction layer over most Vulkan primitives, so the renderer never touches raw Vulkan handles directly.

# CommandQueue & CommandEncoder

Inspired by the Metal API shape

CommandQueue: Per-frame queue submission and direct queue + swapchain synchronization. Acts like DX12 *DIRECT* queue.

CommandEncoder: automatic image/buffer layout transitions and pipeline barriers from declared usage intents. 

## Per-subresource state

Each image tracks state per (mip, layer). Transitions can target any range. merge groups consecutive subresources with the same source state into a single barrier, so a whole-image transition is one barrier instead of one per subresource.

## Pass usage merging

Two `useImage` calls with the same image and same range in one pass are merged into one entry with OR'd intents. `useBuffer` merges per buffer the same way.

## Allocator

A custom, slab-based Vulkan memory suballocator with two separate pools:
**buffers** and **images**. Keeping them apart avoids `bufferImageGranularity`
hazards and simplifies memory placement.

### Key features

- **Segregated free lists** - Free blocks are bucketed into 32 size classes
  (`log2(size)`) for fast lookup.
- **Slab-based memory management** - Each slab is a single `VkDeviceMemory`
  allocation subdivided into smaller blocks tracked by offset.
- **Immediate coalescing** - Adjacent free blocks are merged on every
  deallocation, bounding fragmentation growth.
- **Alignment-aware splitting** - Requests include worst-case alignment
  padding when selecting a free block, so the aligned allocation always fits
  inside the chosen block.
- **Slab recycling** - Empty inactive slabs are reused before new memory is
  allocated. `shrink()` releases inactive slabs back to the driver.
- **CPU-accessible mapping** - Slabs tagged `CpuToGpu` or `GpuToCpu` are
  mapped at creation time; `MemoryBlock::pMappedData` points at the block's
  offset within the mapped region.

### Allocation strategy

1. **Active slabs** - Scan active slabs of the matching `MemoryType` and
   `memoryTypeIndex`, starting at the requested size class and moving upward
   until a block large enough is found.
2. **Inactive slabs** - If no active slab has space, an inactive slab of the
   matching type is reactivated and reused.
3. **New slab** - Only created when no existing slab can satisfy the request.
   The slab size is `max(configuredSlabSize, size + alignment - 1)`, aligned
   up to `DEFAULT_SLAB_ALIGNMENT`.


### Threading

**Single-threaded only.** No internal synchronization. Callers must
externally synchronize access(mutexes, etc).

### Current limitations

- **Fixed set of memory types.** Only `GpuOnly`, `CpuToGpu`, and `GpuToCpu`
  are supported. Memory selection picks the first compatible type returned
  by the driver and does not distinguish between host-visible variants.
  `CpuToGpu` and `GpuToCpu` are assumed to be host-coherent; no explicit
  flush or invalidate is performed.
- **No defragmentation.** Blocks are never moved. Long-running variable-sized
  workloads can accumulate unusable gaps. A proper fix requires compacting
  live blocks, which in Vulkan means rebinding every affected resource.
- **No dedicated allocations.** Every resource is suballocated, even images
  that would benefit from `VkMemoryDedicatedAllocateInfo`.
- **Alignment leftovers under 16 bytes are dropped.** Gaps smaller than
  `MIN_BLOCK_SIZE` are not tracked. Bounded at 15 bytes per allocation.


**Bindless Textures**

Textures are accessed through a per-scene bindless descriptor array, indexed per-material from the shader. No per-draw descriptor rebinding.

Capacity is MaxTexturesPerScene per scene (currently 4096), validated at startup against the device's per-stage sampled-image limit. The descriptor pool is sized for MaxScenes * (MaxTexturesPerScene + 1) to cover all scenes plus one shadow map each.

Slots are allocated monotonically; textures are not unloaded at runtime and are freed only when the whole RenderScene is destroyed.

# Error policy

Every component in the renderer throws std::runtime_error on failure. main is the only catch point. Failure of any subsystem - allocator, buffer, image, swapchain - propagates to main, which prints the error and exits.

# Testing
Tested on: NVIDIA RTX 40-series (Vulkan 1.4), AMD integrated GPU (Vulkan 1.3), Windows only.

# Important

For debugging Vulkan Context and seeing Validation layer use function: tungol::event::subscribe_graphics_default_logger();

## Design notes

A multi-queue design (graphics + transfer + compute) was prototyped with timeline semaphores, but measured ~20% slower than a single queue for this workload. The transfer work is dependent on graphics (no overlap), the sync overhead exceeded the parallelism, and the per-frame release/acquire barriers are heavier than the single-queue barriers. Even when renderer will require multithreaded implementation, synchronization will still use fences and binary semaphores, not timeline semaphores - fences give the CPU finer control (poll vs. wait) and binary semaphores keep the queue chain visible in the submit rather than hidden in a counter.

# Future work

Complete the `EventBus` implementation for the remaining event types, arguments, etc.

[3] Rethink the bindless texture approach to allow uploading and unloading textures independently.

[2] Rethink the rendering model: the core idea stays the same, but the scope becomes global instead of per-scene, targeting streaming.

# Current limitation

Asset loading is currently fully synchronous: `stb_image` decodes textures and `stb_resize` generates mips on the CPU, followed by a blocking `fence.wait()` in the `CopyContext`. This causes a noticeable stall during scene initialization. 

[1] The plan is to rethink asset loading to minimize stalling and reduce extraction complexity, potentially by moving decoding to a background thread and batching GPU uploads.