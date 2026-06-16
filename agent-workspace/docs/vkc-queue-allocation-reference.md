# vkc Queue 角色与分配设计 —— 多引擎查证综合

确定 vkc 的 `QueueRole` 枚举与 queue 分配算法。结论**不基于单一引擎**,而是横向查证
Granite / UE5 / O3DE / NVRHI / D3D12 + AMD/Khronos 官方建议后综合得出。
本文是**参考文档(查证沉淀)**,不是一次性;后续 queue 相关决策回看这里。

配套:`vkc-create-device-design.md`(create_device 收 `{family,count}` 机械描述)、
`vkc-device-queue-creation-guide.md`(DeviceContext 编排)。本文替代 guide §1 里那套
「QueueRole 推导理想数」的旧表述——以本文的 3-类共识为准。

---

## 0. 结论速览

1. **核心 queue 角色只有 3 个:graphics / compute / transfer。** 这不是个人口味,是
   D3D12 API 级硬约束(Direct/Compute/Copy)+ 三家跨 API 大引擎(UE5/O3DE/NVRHI)的一致收敛。
2. **每个 flavor 默认 1 条 queue**(role:flavor:queue = 1:1:1)。AMD 实证:graphics 多给无用、
   compute 1 条够、copy 1 条有收益。全量分配(如 NV 的 16 条 graphics)是反模式。
3. **分配算法 = Granite 的 `find_vacant_queue`**:ignore_flags first-match + 空位递减 + 退化阶梯。
   经验证、与 3-类模型完全兼容。
4. **视频(decode/encode)不进核心 queue 模型**(对齐 UE5/O3DE/NVRHI),未来做视频时单独子系统。
   Granite 的 extension 门控模式记录为扩展路径(§7)。
5. **present 不在 queue 分配阶段查**(偏离 Granite),按 vkc 不变量推迟到 swapchain(§6)。

---

## 1. 多引擎横向对比

| 来源 | 抽象 | 核心类别 | 视频 | 多 compute/transfer |
|---|---|---|---|---|
| Granite(个人,Khronos 贡献者) | `QueueIndices` | graphics/compute/transfer | decode/encode 进枚举,ext 门控,缺则 unavailable | 单 compute、单 transfer |
| UE5 RHI | `FQueue::EType` | Graphics/Compute/Copy(3) | 不在通用 queue 模型 | 单 async-compute pipe |
| O3DE Atom RHI | `HardwareQueueClass` | Graphics/Compute/Copy(3) | 不在 queue 模型 | 单 |
| NVRHI(NVIDIA) | `CommandQueue` | Graphics/Compute/Copy(3) | 不在 queue 模型 | 单 |
| D3D12(API 基准) | engine type | Direct/Compute/Copy(3) | 独立 API | app 控 |
| AMD GPUOpen(厂商) | —— | 3 | —— | **1 compute + 1 copy 是甜点,别更多** |

**最强信号**:所有要同时抽象 D3D12 + Vulkan 的 RHI(UE5/O3DE/NVRHI)都收敛到正好 3 类。
因为 D3D12 把引擎模型硬定为 3 种,这 3 类是两 API 的交集,也是 GPU 通用并行引擎的真实数目。

---

## 2. 核心概念:flavor / family / role / queue 数

这是最容易混的地方,先厘清四个词的关系,否则低端设备的行为会想不通。

| 概念 | 侧 | 含义 | 例 |
|---|---|---|---|
| **flavor** | 需求 | 能力类别「我要一条什么样的引擎」 | graphics / compute / transfer |
| **role** | 需求 | 一条具体的 queue 需求单元 | eGraphics / eCompute / eTransfer |
| **family** | 供给 | 设备实际暴露的物理队列族 | NV 5080 的 6 个 family |
| **queue 数** | 落地 | 某 family 实际建几条 queue | 空位递减算出 |

### 2.1 flavor → family 是多对一,运行期解析

**「三类对应三个 family」只在高端成立,不是普适。** flavor 在具体设备上解析到哪个 family
是运行期决定的:

- **高端(NV 5080)**:graphics / compute / transfer 三个 flavor 各落到不同的专用 family。
- **低端(iGPU 单族 `Graphics|Compute|Transfer`)**:三个 flavor **全部落到同一个 family**。

所以 flavor 是「需求侧的能力类别」,family 是「供给侧的物理资源」,中间靠分配算法解析。

### 2.2 每类给几条 queue?答案:1 条

在 3-类模型里 **role : flavor : queue = 1 : 1 : 1**:

- 一个 flavor 对应一个 role,一个 role 就是「我要一条这种引擎的 queue」的需求单元。
- 每个 flavor 的理想 queue 数 = 映射到它的 role 个数 = **1**。

这正被 AMD 数据背书:graphics 多给无用(单 geometry frontend 串行化)、compute「1 条够、
没见过多条的明显收益」、copy「1 条专用 DMA 有收益」。**1/1/1 是 profile 出来的实证甜点。**

> 想要某 flavor 多条怎么办?**加一个该 flavor 的 role。** role 是 queue 数的计量单位——
> 加 role,理想数 +1。但 baseline 各 1 条。这也是为什么不用手维护 `{graphics:1,compute:1}`
> 这种表:数一下映射到该 flavor 的 role 个数即可(§4 的 `ideal_queue_count`)。

### 2.3 实际建几条 ≠ 理想几条

理想数是需求,实际建几条由空位递减 + 硬件封顶决定:

- **高端**:每个 flavor 落到不同 family,每 family 建 1 条 → 三条独立引擎全点亮。
- **低端**:三个 flavor 落到同一 family,graphics 领走 queue 0,compute/transfer 发现没空位
  → 别名到 graphics 的 queue。物理上「1 个 family、1 条 queue、3 个 role 共享」。

一句话:**flavor 是需求(各要 1 条),family 是供给(运行期解析、可合并),空位递减把需求
落到供给上并自动封顶硬件上限。**

---

## 3. QueueRole 枚举

```cpp
// context/enums.h
enum class QueueRole : uint8_t
{
    eGraphics = 0,   // = D3D12 Direct / UE Graphics。present 候选,万能兜底,必须是 0
    eCompute,        // = async compute。单条(AMD/UE 实证:1 条足够)
    eTransfer,       // = D3D12 Copy。单条专用 DMA(AMD:1 条 copy 有收益)
    // —— 视频不进核心模型(对齐 UE5/O3DE/NVRHI)。未来做视频时见 §7 ——
};
```

设计约定:

- **`eGraphics` 必须 value 0**:分配按枚举顺序,graphics 族在所有设备上一定存在,它是所有
  退化的最终兜底,必须最先分配。
- **只有 3 个核心 role**:多 compute 流 / 第二 graphics queue 没有任何主流引擎默认这么做,不加。
- **`eSparseBinding` / `eProtected` 不进 role**:它们是修饰位(挂在任意 queue 上),不是引擎,
  作为「对某 queue 的能力要求」在资源/swapchain 层按需校验。

---

## 4. 理想数推导(从枚举自动数,不手维护)

```cpp
constexpr vk::QueueFlagBits queue_role_flavor(QueueRole role) noexcept
{
    switch (role) {
        case QueueRole::eGraphics: return vk::QueueFlagBits::eGraphics;
        case QueueRole::eCompute:  return vk::QueueFlagBits::eCompute;
        case QueueRole::eTransfer: return vk::QueueFlagBits::eTransfer;
    }
    return vk::QueueFlagBits::eGraphics;   // 不可达
}
```

`ideal_queue_count` 不必单独写——3-类模型下每 flavor 恒为 1,直接在分配里按 role 逐个领即可。
若将来加同 flavor 的第二个 role,再引入「数枚举」的 `ideal_queue_count`(guide §1.2 的写法)。
现阶段保持简单。

---

## 5. 分配算法:Granite `find_vacant_queue`(ignore_flags + 空位递减)

照搬 Granite 的验证形态。核心是一个 lambda:在「有 required 能力、没有 ignore 能力、且该族
还有空位」的 family 里领一条 queue,领到就 `queueCount--` + `offset++`——保证同族不同 role
自动错开 index,且天然封顶硬件上限。

```cpp
// DeviceContext.cpp 匿名 ns。families 是 getQueueFamilyProperties 的可变副本(queueCount 会被递减)
struct QueueAssignment { uint32_t family; uint32_t index; };

// family → 该族已领取的 queue 数(= 要创建的 queue 数 = 下一个可领 index)
std::vector<uint32_t> queue_offsets(families.size(), 0);

auto find_vacant_queue = [&](QueueAssignment & out, vk::QueueFlags required,
                             vk::QueueFlags ignore) -> bool {
    for (uint32_t i = 0; i < families.size(); ++i) {
        if (families[i].queueFlags & ignore) { continue; }            // 排除带 ignore 能力的族
        if (families[i].queueCount == 0) { continue; }                // 该族已发完
        if ((families[i].queueFlags & required) != required) { continue; }
        out = { i, queue_offsets[i] };
        --families[i].queueCount;                                     // 占一条
        ++queue_offsets[i];
        return true;
    }
    return false;
};
```

退化阶梯(逐条对齐 Granite,role → 第一/第二选择 → 兜底):

```cpp
std::array<std::optional<QueueAssignment>, enum_count_v<QueueRole>> role_queues;
using QF = vk::QueueFlagBits;

// graphics:GRAPHICS|COMPUTE 的族(无则硬错误)。注意此处不查 present(§6)
QueueAssignment graphics;
if (not find_vacant_queue(graphics, QF::eGraphics | QF::eCompute, {})) {
    return make_error_code(errc::no_suitable_queue_family);
}
role_queues[std::to_underlying(QueueRole::eGraphics)] = graphics;

// compute:专用 async(COMPUTE 且非 GRAPHICS)→ 任意 COMPUTE → 别名 graphics
QueueAssignment compute;
if (find_vacant_queue(compute, QF::eCompute, QF::eGraphics) or
    find_vacant_queue(compute, QF::eCompute, {})) {
    role_queues[std::to_underlying(QueueRole::eCompute)] = compute;
} else {
    role_queues[std::to_underlying(QueueRole::eCompute)] = graphics;  // 别名
}

// transfer:专用 DMA(TRANSFER 且非 GRAPHICS|COMPUTE)→ 退到 compute 族 → 别名 compute
QueueAssignment transfer;
auto & compute_q = *role_queues[std::to_underlying(QueueRole::eCompute)];
if (find_vacant_queue(transfer, QF::eTransfer, QF::eGraphics | QF::eCompute) or
    find_vacant_queue(transfer, QF::eCompute, QF::eGraphics)) {
    role_queues[std::to_underlying(QueueRole::eTransfer)] = transfer;
} else {
    role_queues[std::to_underlying(QueueRole::eTransfer)] = compute_q;  // 别名 compute
}
```

> transfer 兜底优先退到 **compute** 而非 graphics(Granite 策略):专用 DMA 缺失时,用 compute
> 队列做传输比占用 graphics 队列更不易阻塞主渲染。这修正了早期草稿里「transfer 退化到 graphics」的错。

产物两份:

- `queue_offsets`(family → 建几条)→ 转成 `{family, count}` 喂 `bs::DeviceCreateInfo::addQueueFamilyRequest`。
- `role_queues`(role → `{family, index}`)→ device 建好后 `device.getQueue(family, index)` 套 `QueueContext`。

> ⚠️ 别名是安全的:多个 role 指向同一 `{family, index}`,只是后续 `getQueue` 拿到同一个
> `vk::Queue`、共用一个 `QueueContext`。`QueueContext` 是唯一提交漏斗,共享不破坏同步。
> 注意 `role_queues` 存的是值(`QueueAssignment`)不是指针,无 vector 重分配失效问题。

---

## 6. 偏离 Granite:present 不在分配阶段查

Granite 在 `find_vacant_queue` 里对 graphics 候选查 `getSurfaceSupportKHR`,把 graphics 族绑死
到 surface。**vkc 刻意不这么做**:核心不变量是 vk_core 不依赖 window/surface,present-family
的解析推迟到 `Swapchain::create`(见 guide §0)。所以本算法的 graphics 选择只看 queue flags,
绝不收 `vk::SurfaceKHR` 参数。

代价:理论上存在「graphics 族不支持 present」的设备,此时要在 swapchain 阶段回退处理。实践中
桌面/移动主流设备的 graphics 族都支持 present,这个偏离的风险极低,换来的是干净的 surface 解耦。

---

## 7. 未来扩展:视频与新引擎

视频(decode/encode)、光流等专用固定功能引擎,**当前不进核心 queue 模型**(对齐
UE5/O3DE/NVRHI——它们都把视频放在专门子系统,不污染通用 queue 抽象)。

将来要做视频时,按 Granite 的模式加(记录在此备查):

1. `QueueRole` 追加 `eVideoDecode` / `eVideoEncode`(排在核心 3 个之后)。
2. **extension 门控**:只有对应扩展(`VK_KHR_video_queue` + codec 扩展)经 manifest 启用时才请求。
3. **无对应 family 则 unavailable**:`find_vacant_queue` 失败时 role 槽位保持 `nullopt`,
   消费者查询拿到空——专用引擎**绝不退化到 graphics**(graphics 无法模拟视频/光流)。
4. 视频 family 解析还需 `VkQueueFamilyVideoPropertiesKHR` 查 codec 支持,比核心复杂,建议
   届时独立成 video 子系统而非塞进核心分配。

核心 3 类(graphics/compute/transfer)永不变;新引擎一律「专用 role + ext 门控 + 缺则 unavailable」。

---

## 8. 两个设备走查

**NV 5080**(6 family:f0=G|C|T、f1=T、f2=C|T、f3=T|Decode、f4=T|Encode、f5=T|OpticalFlow):

| role | find_vacant_queue | 落到 |
|---|---|---|
| eGraphics | `G\|C`, ignore 无 → f0 | {0,0} |
| eCompute | `C`, ignore `G` → f2(专用 compute) | {2,0} |
| eTransfer | `T`, ignore `G\|C` → f1(专用 DMA) | {1,0} |

建 3 条 queue(f0/f1/f2 各 1),三条独立引擎点亮。video/optical-flow 的 family 不被核心模型
触碰(无对应 role),完全不浪费——也不会去建那些 queue。

**低端 iGPU**(单 family f0 = `G|C|T`,queueCount=1):

| role | find_vacant_queue | 落到 |
|---|---|---|
| eGraphics | `G\|C` → f0,领 queue 0,f0.count→0 | {0,0} |
| eCompute | `C` ignore `G` → 无;`C` → f0 但 count=0 → 失败 → 别名 graphics | {0,0} |
| eTransfer | 专用→无;compute族→f0 count=0 失败 → 别名 compute | {0,0} |

建 1 条 queue,3 个 role 共享。**任意 family 布局都落得下去**——这就是「无论 physical 设备是
什么都成功落到 role」的保证。

---

## 9. 落地清单

1. **`context/enums.h`**:`QueueRole` 改为 §3 的 3 值版(现状是旧的 4 值
   `eGraphics/eAsyncCompute/eCompute/eTransfer`);加 `queue_role_flavor`。
2. **`DeviceContext.cpp`**:§5 的 `find_vacant_queue` + 退化阶梯,产出 `{family,count}` 与
   `role_queues`。这是 guide §7.1-7.4 的具体落地(以本文算法为准,替代 guide 里的旧 collapse ladder)。
3. **`QueueContext`**:device 建好后按 `role_queues` 取 queue(guide §8 的 create 已就绪)。
4. 不改 CMake(GLOB)。

---

## 10. 一句话总结

核心 queue 角色 = **graphics / compute / transfer 三类**(D3D12 硬约束 + UE5/O3DE/NVRHI 共识),
每类 **1 条 queue**(role:flavor:queue = 1:1:1,AMD 实证甜点),分配用 **Granite 的
`find_vacant_queue`**(ignore_flags first-match + 空位递减 + 退化阶梯),**present 推迟到 swapchain**,
**视频等专用引擎不进核心模型**(留作 ext 门控的未来扩展)。flavor 是需求、family 是供给、空位递减
把需求落到供给并封顶硬件——任意设备都能落得下去。


