#pragma once
//
// VkDebugLabel：vkCmdBeginDebugUtilsLabelEXT / End 的 RAII 包装。
//
// 用途：给 nsys / RenderDoc 等外部 profiler 在 Vulkan command buffer timeline 上
// 标识关键段（CullDispatch / DGCExecute 等），抓 GPU duration 时可定位到 segment
// 级数据，做 query pool 数据的双源认证（chap05 §5.5.6）。
//
// 用法：
//   {
//       lcf::benchmark::VkDebugLabel label(cmd, "GpuDriven::CullDispatch");
//       cmd.dispatch(...);
//   }  // 离开作用域自动 vkCmdEndDebugUtilsLabelEXT
//
// 实现：第一次构造时 lazily 通过 vkGetDeviceProcAddr 加载函数指针，缓存。
// 这样既不依赖 vulkan-hpp wrapper（IS_DISPATCHED 的 link 一致性问题），
// 也不依赖 vulkan.h 的全局符号（在 VK_NO_PROTOTYPES 下不存在）。
//
// 要求：
//   - VK_EXT_debug_utils instance extension 已启用（requirements.cpp 全配置启用）
//   - 调用前必须先 init(VkDevice)：在 main.cpp benchmark 启动后立即调一次
//

#include <vulkan/vulkan.hpp>
#include <atomic>

namespace lcf::benchmark {

    class VkDebugLabel
    {
    public:
        // 一次性初始化函数指针。device 取自 VulkanContext.getDevice()。
        // 加载失败时，构造/析构变成 no-op，不影响主流程。
        static void init(vk::Device device) noexcept
        {
            if (s_initialized.load(std::memory_order_acquire)) { return; }
            if (not device) { return; }
            s_pfn_begin = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
                device.getProcAddr("vkCmdBeginDebugUtilsLabelEXT"));
            s_pfn_end   = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
                device.getProcAddr("vkCmdEndDebugUtilsLabelEXT"));
            s_initialized.store(true, std::memory_order_release);
        }

        VkDebugLabel(vk::CommandBuffer cmd, const char * name) noexcept
            : m_cmd(cmd)
        {
            if (not s_pfn_begin) { return; }
            VkDebugUtilsLabelEXT info{};
            info.sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
            info.pLabelName = name;
            info.color[0] = 0.30f; info.color[1] = 0.55f;
            info.color[2] = 0.85f; info.color[3] = 1.0f;
            s_pfn_begin(static_cast<VkCommandBuffer>(m_cmd), &info);
        }

        ~VkDebugLabel() noexcept
        {
            if (not s_pfn_end) { return; }
            s_pfn_end(static_cast<VkCommandBuffer>(m_cmd));
        }

        VkDebugLabel(const VkDebugLabel &)             = delete;
        VkDebugLabel & operator=(const VkDebugLabel &) = delete;

    private:
        vk::CommandBuffer m_cmd;

        // 函数指针缓存（init() 一次写，所有线程读；指针写入与 atomic flag 配对）。
        inline static PFN_vkCmdBeginDebugUtilsLabelEXT s_pfn_begin = nullptr;
        inline static PFN_vkCmdEndDebugUtilsLabelEXT   s_pfn_end   = nullptr;
        inline static std::atomic<bool>                s_initialized{false};
    };

}  // namespace lcf::benchmark
