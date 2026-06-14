# vkc DeviceRole / QueueRole 枚举设计

评估 `DeviceRole` / `QueueRole` 的取值是否合理,并参考 `libs/image/include/image/image_enums.h`
的 encode/decode 模式,设计枚举携带的额外元信息。一次性文档,用完即弃。给文档不改代码。

---

## 0. 一个贯穿全文的硬约束:下标方式

当前代码这样用这两个枚举:

```cpp
std::array<DeviceContext *, enum_count_v<enums::DeviceRole>> m_device_table {};
std::array<QueueContext *, enum_count_v<enums::QueueRole>>   m_mapped_queue_contexts {};
return *m_mapped_queue_contexts[std::to_underlying(role)];   // ← 用枚举值当下标
```

`std::to_underlying(role)` 当数组下标,**只在枚举值是稠密的 0,1,2,3… 时成立**。

image_enums 的 encode 会产生**稀疏**值(`eRGBA8Uint = encode(...) = 大数`),它能这么做是因为
**ImageFormat 从不被 magic_enum 扫描、也从不当数组下标**,只用 `enum_decode::` 函数按位还原。

所以这里有个二选一:

| | 枚举值 | 元信息怎么带 | 数组下标 |
|---|---|---|---|
| **方案 A** | 稠密 0,1,2,3 | 旁路的 `decode` switch | `std::to_underlying(role)` ✅ |
| **方案 B** | encode 打包(稀疏) | 直接按位 decode 枚举值 | **必须改用** `magic_enum::enum_index(role)` |

`magic_enum::enum_index<E>(value)` 返回枚举量在**枚举量列表里的 0 基序号**(稠密),无论枚举值
多稀疏都能当合法下标。代价:比 `to_underlying` 多一次查找(magic_enum 内部是排序数组 +
二分,O(log n))——对 role 表这种冷路径(创建时填一次,运行时偶尔查)完全可忽略。

> 还有个 magic_enum 的范围约束:默认只扫描 `[MAGIC_ENUM_RANGE_MIN, MAX] = [-128, 127]`。
> encode 后的值必须 ≤ 127(uint8_t 正区间),否则 `enum_count` / `enum_values` /
> `enum_index` 全失效。下面的 QueueRole 编码刻意把值压在 ≤ 16,留足余量。

结论先行:**QueueRole 适合方案 B**(flavor 是它的内在属性,值得打包),**DeviceRole 建议留方案 A**
(只有 2 个值,几乎没有可打包的结构化元信息,encode 是过度设计)。下面分别论证。

---

## 1. DeviceRole —— 取值评估

```cpp
enum class DeviceRole : uint8_t { eMain = 0, eCompute };
```

- `eMain`:主图形 + 呈现设备(独显)。
- `eCompute`:异步/离线 compute 设备 —— 双 GPU 上可落到集显或第二张卡,单 GPU 上别名 `eMain`。

**合理性:对当前目标够用。** 它表达的是「设备承担的工作角色」,而 eMain/eCompute 正好覆盖
你这台独显 + 集显机器的「主渲染 + 旁路 compute」场景。

可能的扩展(现在不必加,列出供权衡):

| 候选角色 | 含义 | 是否建议现在加 |
|---|---|---|
| `eDisplay` | 多 GPU 下专门驱动显示输出的设备(与渲染设备分离) | ❌ 没有多屏异 GPU 呈现需求前不加 |
| `eTransfer` | 专门做 PCIe 上传/下载的设备 | ❌ 跨设备传输是 DeviceRole 之上的 policy,不是设备角色 |

**不要**给 DeviceRole encode「默认设备类型偏好」(如 eMain→discrete、eCompute→integrated)。
设备类型偏好是**用户可配 policy**,已经在 `DeviceContextCreateInfo::setPreferredDeviceType` 里。
把它 encode 进枚举,等于把 policy 焊死进 vocabulary 类型 —— 违背 vkc「不夹带 engine policy」
的原则。DeviceRole 只回答「这个设备扮演什么角色」,不回答「该挑哪种硬件」。

> 若将来真想让 DeviceRole 带一点内在元信息,唯一说得通的是「该角色必须具备的队列能力」
> (eMain 必须有 graphics、eCompute 必须有 compute),用来在选设备时硬过滤掉缺该能力族的设备。
> 但所有实际 GPU 都同时有 graphics+compute+transfer,这个过滤几乎永不触发,收益≈0。
> **建议 DeviceRole 维持纯 enum(方案 A),不 encode。**

---

## 2. QueueRole —— 取值评估

```cpp
enum class QueueRole : uint8_t { eMainGraphics = 0, eSubGraphics, eCompute, eTransfer };
```

**合理,且确实有可打包的内在元信息。** 每个 QueueRole 天然绑定两类结构化信息:

1. **flavor** —— 它需要的队列能力(graphics / compute / transfer)。这是 collapse ladder
   分配队列的依据(见队列创建指南 §1)。
2. **flavor 内序号** —— 同 flavor 下这是第几个角色(eMainGraphics=0、eSubGraphics=1)。
   这正是「理想队列数」的来源:某 flavor 下最大序号 +1 = 该族要创建几个队列。

现在这两项信息散落在手写的 `queue_role_flavor()` switch 和 `ideal_queue_count()` 循环里。
encode 模式可以把它们**直接打包进枚举值**,让 flavor / 序号成为枚举的可解码字段,消灭手写表。

### 2.1 合理性结论

- 4 个角色覆盖了主流水线需求:主图形、副图形(并行录制/异步图形)、compute、transfer。**够用。**
- 可选扩展(现在不必加):`eAsyncCompute`(与 eCompute 区分优先级)、`eSparseBinding`
  (`vk::QueueFlagBits::eSparseBinding`)、`eVideoDecode/eEncode`。加它们时,encode 方案的好处
  显现 —— 新角色只要在枚举值里编好 flavor + 序号,`queue_role_flavor` / `ideal_queue_count`
  自动正确,无需改任何 switch。

---

## 3. QueueRole 的 encode/decode 设计

仿 image_enums:`internal::encode` 在编译期打包,`enum_decode::` 按位还原。

### 3.1 字段布局(uint8_t,值 ≤ 16,magic_enum 安全)

```
 bit:  7 6 5 4 3   2 1 0
       └───┬───┘   └─┬─┘
        序号(0..31)  flavor(0..7)
```

- 低 3 位:flavor 索引(graphics/compute/transfer,留到 8 种)。
- 高 5 位:该角色在其 flavor 内的序号(0 基)。

> 这样编出来的最大值:eSubGraphics = 序号1·graphics → `(1<<3)|0 = 8`,远 < 127,magic_enum
> 范围安全。flavor 放低位是有意的:让「同 flavor 不同序号」的值相邻成一段,decode flavor 只需
> `& 0x7`。

### 3.2 内部 flavor 枚举 + encode

```cpp
// context/enums.h
namespace internal {
    enum class QueueFlavor : uint8_t { eGraphics = 0, eCompute, eTransfer };

    inline constexpr uint8_t encode(QueueFlavor flavor, uint8_t index_in_flavor) noexcept
    {
        return (index_in_flavor << 3) | std::to_underlying(flavor);
    }
}

enum class QueueRole : uint8_t  // 低 3 位 flavor,高 5 位 flavor 内序号
{
    eMainGraphics = internal::encode(internal::QueueFlavor::eGraphics, 0),  // = 0
    eSubGraphics  = internal::encode(internal::QueueFlavor::eGraphics, 1),  // = 8
    eCompute      = internal::encode(internal::QueueFlavor::eCompute,  0),  // = 1
    eTransfer     = internal::encode(internal::QueueFlavor::eTransfer, 0),  // = 2
};
```

注意枚举值变成 `{0, 8, 1, 2}` —— **稀疏**。这就是 §0 说的:从此 role 表下标必须用
`magic_enum::enum_index(role)`,不能再用 `std::to_underlying(role)`。

### 3.3 decode

```cpp
namespace enum_decode {
    inline constexpr internal::QueueFlavor get_queue_flavor(QueueRole role) noexcept
    {
        return static_cast<internal::QueueFlavor>(std::to_underlying(role) & 0x7);
    }
    inline constexpr uint8_t get_flavor_index(QueueRole role) noexcept
    {
        return std::to_underlying(role) >> 3;
    }
    // flavor → vk::QueueFlagBits(对外仍用 vk 原生类型,不暴露 internal::QueueFlavor)
    inline constexpr vk::QueueFlagBits to_vk_queue_flag(internal::QueueFlavor flavor) noexcept
    {
        switch (flavor) {
            case internal::QueueFlavor::eGraphics: return vk::QueueFlagBits::eGraphics;
            case internal::QueueFlavor::eCompute:  return vk::QueueFlagBits::eCompute;
            case internal::QueueFlavor::eTransfer: return vk::QueueFlagBits::eTransfer;
        }
        return vk::QueueFlagBits::eGraphics;
    }
    inline constexpr vk::QueueFlagBits queue_role_flavor(QueueRole role) noexcept
    {
        return to_vk_queue_flag(get_queue_flavor(role));
    }
}
```

### 3.4 理想队列数 —— 从 encode 直接算

队列创建指南 §1.2 原本用「遍历所有 role、数同 flavor 个数」。有了 flavor 内序号,等价于
「该 flavor 下最大序号 + 1」:

```cpp
namespace enum_decode {
    inline constexpr uint32_t ideal_queue_count(internal::QueueFlavor flavor) noexcept
    {
        uint32_t count = 0;
        for (auto role : magic_enum::enum_values<QueueRole>()) {
            if (get_queue_flavor(role) == flavor) {
                count = std::max(count, uint32_t(get_flavor_index(role)) + 1);
            }
        }
        return count;
    }
}
// graphics → 2(eSubGraphics 序号 1),compute → 1,transfer → 1
```

> 用 `max(序号)+1` 而不是 `count(同 flavor)`,语义更准:它表达「这个 flavor 最多被第几个角色
> 引用」。即使将来角色定义跳号(eMainGraphics=0, eThirdGraphics=2 而无序号 1),队列数也对。
> 当前无跳号,两种算法结果相同。

### 3.5 encode 带来的连锁改动

| 位置 | 原来 | 改为 |
|---|---|---|
| `DeviceContext::getQueueContext` | `m_mapped[std::to_underlying(role)]` | `m_mapped[magic_enum::enum_index(role).value()]` |
| 队列创建指南 §7.4 ladder | 同上,所有 `to_underlying(QueueRole)` 下标 | `magic_enum::enum_index` |
| `queue_role_flavor` | 手写 switch | `enum_decode::queue_role_flavor`(decode) |
| `ideal_queue_count(vk::QueueFlagBits)` | 入参 vk flag | 入参 `internal::QueueFlavor`(或加一层 vk→flavor 适配) |

> DeviceRole 不 encode,它的 `to_underlying` 下标保持不变。只有 QueueRole 受影响。
> 建议封装一个小工具 `queue_role_to_index(role)` / `device_role_to_index(role)`,把
> 「QueueRole 用 enum_index、DeviceRole 用 to_underlying」的差异藏进去,调用点统一。

---

## 4. 该不该上 encode?权衡

encode 不是免费的。诚实对比:

| | 不 encode(方案 A) | encode(方案 B) |
|---|---|---|
| flavor 来源 | 手写 `queue_role_flavor` switch | decode 枚举值低 3 位 |
| 理想队列数 | 手写遍历 | decode max(序号)+1 |
| 加新角色 | 改 switch + 可能改 ideal | 只在枚举值里编好 flavor+序号,decode 自动对 |
| 数组下标 | `to_underlying`(直接) | `enum_index`(多一次二分查找) |
| 可读性 | 枚举值 0,1,2,3 一目了然 | 值变 0,8,1,2,需对照位布局理解 |
| 出错面 | switch 漏 case 编译器可告警 | 位布局 / magic_enum 范围 / 下标方式三处都得对 |

**核心判断:encode 的收益在「加角色时 switch 自动正确」,代价在「下标方式被迫改 + 可读性下降」。**

image_enums 用 encode 是因为 ImageFormat 有 **6×5=30 种组合**、且 channel count / data type /
color space 是高频解码的真实属性 —— 打包收益巨大。QueueRole 只有 **4 个值**,flavor 的 switch
就 4 行,「收益」相当有限。

### 我的建议

**两种都成立,取决于你对未来角色增长的预期:**

- **若 QueueRole 会稳定在 4~6 个、很少加** → **方案 A(不 encode)**。一个 `queue_role_flavor`
  switch + `ideal_queue_count` 遍历,稠密值 + `to_underlying` 下标,最简单直白。队列创建指南
  §1 写的就是这版。**默认推荐这个。**

- **若你预期角色会持续扩张**(eAsyncCompute / eSparseBinding / eVideoDecode/Encode…,最终 8~12 个,
  且每次加都要同步改 flavor 和 ideal 两处)→ **方案 B(encode)**。把 flavor+序号 焊进枚举值,
  加角色时「定义即正确」,decode 一次写好永不动。代价是接受 `enum_index` 下标和稀疏值。

无论哪种,有两点是确定的:

1. **DeviceRole 不 encode** —— 2 个值,无结构化元信息,encode 纯属过度设计。
2. **flavor 是 QueueRole 唯一值得打包的信息** —— 不要把 priority、queue count 上限、是否
   present-capable 这类**运行期/policy 信息**也 encode 进去。present 能力依赖 surface(运行期查),
   priority 是 policy,queue 数已由序号隐含。encode 只装「编译期就定死的内在分类」,和 image_enums
   只装 color space/data type 同理。

---

## 5. 一句话总结

- DeviceRole `{eMain, eCompute}`:合理,保持纯 enum。
- QueueRole `{eMainGraphics, eSubGraphics, eCompute, eTransfer}`:合理。flavor + flavor 内序号
  是它的内在元信息,可用 image_enums 式 encode 打包(`(序号<<3)|flavor`),decode 出
  `queue_role_flavor` 和 `ideal_queue_count`,代价是 role 表下标改用 `magic_enum::enum_index`。
- 4 个角色规模下 encode 收益有限,**默认推荐不 encode**;只有当你确定角色会大量增长时才上 encode。
