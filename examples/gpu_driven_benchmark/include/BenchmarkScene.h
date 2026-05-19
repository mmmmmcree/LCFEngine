#pragma once

// BenchmarkScene：三种渲染路径共享的"模型/材质/纹理/UBO/SSBO/bindless DS"集合。
//
// 设计原则（与 chap05 §5.4.1 "三路径共享相同网格、材质、纹理数据、相同描述符集架构"对应）：
//   - 模型 / 材质参数缓冲 / 材质纹理 ID 缓冲 / 纹理图像 / 采样器 / bindless DS
//     全部由本类一次性创建，三个 renderer 持有 BenchmarkScene * 指针共享访问；
//   - 仅"实例数据"会随 setSceneScale(eScene) 重生成；模型本身不重新加载；
//   - 每帧由 renderer 自带的 transfer cmd 负责 commit 共享缓冲，BenchmarkScene
//     在 prepareFrameDataTransfer 中负责把 host shadow 写入对应 buffer 的 pending
//     write segments 并调用 commit。

#include "benchmark_types.h"

#include <cstdint>
#include <memory>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

#include <vulkan/vulkan.hpp>

#include "BoundingVolume.h"
#include "Vulkan/VulkanCommandBufferObject.h"
#include "Vulkan/VulkanContext.h"
#include "Vulkan/VulkanMesh.h"
#include "Vulkan/VulkanPipeline.h"
#include "Vulkan/VulkanTextureManager.h"
#include "Vulkan/ds/VulkanDescriptorSet.h"
#include "Vulkan/ds/VulkanDescriptorSetLayout.h"
#include "Vulkan/memory/VulkanBufferObject.h"
#include "Vulkan/memory/VulkanBufferObjectGroup.h"
#include "Vulkan/memory/VulkanImageObject.h"
#include "ecs/Entity.h"
#include "render_assets/render_assets_fwd_decls.h"
#include "resources/ResourceEntity.h"

namespace lcf {
    class Model;
    class Texture2D;
}
namespace lcf::ecs {
    class Registry;
    class ResourceSystem;
}

namespace lcf::benchmark {

    // 将 scene 档位转换成「每个 mesh 的实例数」。
    // 该数组可由 BenchmarkScene::setInstancePerMeshOverride 在运行时覆盖。
    struct SceneScaleConfig
    {
        std::array<uint32_t, k_scene_count> instance_per_mesh = k_default_instance_per_mesh;
    };

    // BenchmarkScene 是一个"重对象"——一旦 create 完成，模型/纹理/材质常驻
    // GPU 直到析构；多个 renderer 在其生命期内共享它的描述符集与 SSBO。
    class BenchmarkScene
    {
        using VulkanContext        = render::VulkanContext;
        using VulkanCmdBuffer      = render::VulkanCommandBufferObject;
        using VulkanMesh           = render::VulkanMesh;
        using VulkanBufferObject   = render::VulkanBufferObject;
        using VulkanBufferGroup    = render::VulkanBufferObjectGroup;
        using VulkanImageObject    = render::VulkanImageObject;
        using VulkanDescriptorSet  = render::VulkanDescriptorSet;
        using VulkanDsLayout       = render::VulkanDescriptorSetLayout;
        using VulkanPipeline       = render::VulkanPipeline;

    public:
        // 主网格按"模型 → mesh"两级展开存储（与 VulkanRenderer.cpp::MeshPack 同形）。
        struct MeshPack
        {
            std::vector<VulkanMesh> meshes;
        };

    public:
        BenchmarkScene() = default;
        BenchmarkScene(const BenchmarkScene &) = delete;
        BenchmarkScene & operator=(const BenchmarkScene &) = delete;
        BenchmarkScene(BenchmarkScene &&) = delete;
        BenchmarkScene & operator=(BenchmarkScene &&) = delete;
        ~BenchmarkScene();

        // 一次性加载模型、上传 GPU、创建 PerView UBO + 5 路 SSBO + bindless 描述符集。
        // 调用前 context 必须已 create。
        void create(VulkanContext * context, ecs::Registry & registry);

        // 切换场景档位：重新生成 instance_data_list 与 draw_meta_infos 的 host shadow。
        // 不做实际 commit，commit 推迟到下一次 prepareFrameDataTransfer 内由 transfer cmd 完成。
        void setSceneScale(eScene scene);
        eScene getCurrentScene() const noexcept { return m_current_scene; }

        // 允许 CLI 用 --instance A=...,B=... 覆盖默认 SceneScaleConfig。
        void setInstancePerMeshOverride(const SceneScaleConfig & config) noexcept;

        // 为某个 renderer 在它已经 begin 的 transfer cmd 上 commit 共享缓冲。
        // 注意 PerView UBO 的"角度数据"在该函数里根据相机重建，所以传入 camera。
        // visible_instance_payload 由 renderer 准备好（CPU 路径已剔除；GPU 路径
        // 由 cull.comp 写入则传 nullptr 即可，不在此 commit）。
        void prepareFrameDataTransfer(
            VulkanCmdBuffer & transfer_cmd,
            const ecs::Entity & camera,
            std::pair<uint32_t, uint32_t> render_extent);

        // 仅更新 CameraData host shadow（frustum 等），不做 buffer 写入。
        // CPU 路径在 prepareFrameDataTransfer 之前先调它拿到 frustum 做剔除。
        void updateCameraShadow(const ecs::Entity & camera,
                                std::pair<uint32_t, uint32_t> render_extent);

        // CPU 路径每帧剔除前先调用：把 m_draw_meta_infos[i].instance_count 复位为
        // 原始值（来自 m_draw_meta_infos_template），避免上一帧覆写后逐帧污染。
        void resetDrawMetaToOriginal();

        // 由 CPU 路径在剔除后调用：覆盖 host shadow 的 draw_meta_infos[i].instance_count
        // 为剔除后的可见数；不改 first_instance（仍指向原 instance pool 的起始）。
        // 这与 cull.comp 在 GPU 路径下写入 draw_meta_infos 的位置一致，保证两路径
        // 在 graphics shader 端读到的内容语义相同。
        void overrideDrawMetaInstanceCount(uint32_t draw_index, uint32_t new_count);

        // CPU 路径还需要写入 visible_instances（带 1 字 head + uint[]）。
        // **关键**：与 cull.comp 等价的语义要求把可见 instance_id 写到
        // visible_instance_ids[first_instance + offset_within_mesh]，因为
        // vertex shader 通过 gl_InstanceIndex (= firstInstance + InstanceID) 索引该数组。
        // 因此提供「按槽位写入」接口而非顺序追加。
        void resizeVisibleInstancesBuffer();           // 在剔除前调一次：扩容到 m_total_instance_count
        void writeVisibleInstanceAt(uint32_t slot,
                                    uint32_t instance_id);   // 直接写入 m_visible_instances[slot]
        void clearVisibleInstancesShadow() noexcept;   // 跨 path 切换时清掉 host shadow

        // ------------- 共享访问接口 -------------

        VulkanContext * getContext() const noexcept { return m_context_p; }

        // PerView 描述符集（set 0）。三个 renderer 共用同一个对象。
        const VulkanDescriptorSet & getPerViewDescriptorSet() const noexcept { return m_per_view_descriptor_set; }
        const VulkanDsLayout      & getPerViewDescriptorSetLayout() const noexcept { return m_per_view_descriptor_set_layout; }

        // 5 路 BindlessBuffer SSBO 组（set 1）。
        VulkanBufferGroup       & getBindlessBufferGroup()       noexcept { return m_per_renderable_ssbo_group; }
        const VulkanBufferGroup & getBindlessBufferGroup() const noexcept { return m_per_renderable_ssbo_group; }

        // mesh / object 信息查询。
        const std::vector<MeshPack> & getMeshPacks() const noexcept { return m_mesh_packs; }
        uint32_t getObjectCount() const noexcept { return m_object_count; }
        uint32_t getInstanceCountPerMesh() const noexcept { return m_current_instance_per_mesh; }
        uint32_t getTotalInstanceCount() const noexcept { return m_total_instance_count; }

        // 各 renderer 在 CPU 端做视锥剔除时用到的源数据。
        std::span<const ObjectData>             getObjectDataList() const noexcept { return m_object_data_list; }
        std::span<const InstanceData>           getInstanceDataList() const noexcept { return m_instance_data_list; }
        std::span<const BoundingSphere<float>>  getBoundingSpheresList() const noexcept { return m_bounding_spheres_list; }
        std::span<const DrawMetaInfo>           getDrawMetaInfos() const noexcept { return m_draw_meta_infos; }

        // 各 renderer 把"已剔除可见实例 id"列表写入此函数指定的输出缓冲。
        // 仅 CPU 路径用到；GPU 路径由 cull.comp 直接写到 SSBO，不需要此函数。
        // 函数返回 host shadow 的 size（visible instance 总数）以便 renderer 上传。
        uint32_t getVisibleInstancesBufferOffset() const noexcept;

        // PerView UBO 字段访问（用于 renderer 在 draw 阶段用 push_constant 等场景读取）。
        const CameraData & getCameraDataShadow() const noexcept { return m_camera_data_shadow; }

        // 天空盒 pipeline（三 renderer 共享，在 record 阶段先 bind 它画 36 顶点）。
        // 与 main_example 一致：descriptor sets = (PerView, BindlessBuffer, BindlessTexture)；
        // depth = LessOrEqual + write disabled，cullFront，与 .xyww trick 配合把背景推到远平面。
        const VulkanPipeline & getSkyboxPipeline() const noexcept { return m_skybox_pipeline; }

    private:
        void uploadModel(VulkanCmdBuffer & cmd,
                         lcf::Model & model,
                         ecs::ResourceSystem & resource_system);
        void buildBindlessTextureBindings();
        void writePerViewDescriptorSet();
        void writeBindlessBufferDescriptorSet();
        void writeBindlessTextureDescriptorSet();
        void rebuildInstanceData();
        // 一次性初始化天空盒资源：bk.jpg → cube_map（sphere_to_cube pipeline）+ skybox pipeline。
        // 必须在 writeBindlessTextureDescriptorSet 之前调用，因为它要把 cube_map / sampler1
        // 注册到 bindless texture set；该 set 由后者统一 commitUpdate。
        void initSkybox(ecs::ResourceSystem & resource_system);

    private:
        VulkanContext * m_context_p = nullptr;
        ecs::Registry * m_registry_p = nullptr;

        // 模型与材质（持有强引用，析构时自动归还到 ResourceSystem）。
        std::vector<MeshPack>                                          m_mesh_packs;
        std::vector<VulkanBufferObject>                                m_material_params_list;
        std::vector<VulkanBufferObject>                                m_material_texture_ids_list;
        std::unordered_map<const lcf::Texture2D *, uint32_t>           m_texture_id_map;
        render::VulkanBindlessTextureIdTable                           m_bindless_texture_id_table;
        std::vector<std::pair<uint32_t,
            TypedResourceEntity<VulkanImageObject>>>                   m_texture_re_list;
        TypedResourceEntity<VulkanImageObject>                         m_cube_map_re;

        // Skybox pipeline（三 renderer 共享）。在 create() 中由 initSkybox() 创建。
        VulkanPipeline m_skybox_pipeline;

        // PerView UBO + per-view DS（与 VulkanRenderer.cpp 完全等价，但跨三 renderer 共用）。
        VulkanBufferObject m_per_view_uniform_buffer;
        VulkanDsLayout     m_per_view_descriptor_set_layout;
        VulkanDescriptorSet m_per_view_descriptor_set;
        CameraData          m_camera_data_shadow {};

        // 5 路 BindlessBuffer SSBO 组（与 VulkanRenderer.cpp 同顺序）。
        VulkanBufferGroup m_per_renderable_ssbo_group;

        // host 端 shadow（每帧上传到 SSBO；用 std::vector，setSceneScale 时 reserve 足够大）。
        std::vector<ObjectData>             m_object_data_list;
        std::vector<InstanceData>           m_instance_data_list;
        std::vector<BoundingSphere<float>>  m_bounding_spheres_list;
        std::vector<DrawMetaInfo>           m_draw_meta_infos;
        // 原始 draw_meta_infos：在 setSceneScale 末尾保存一份；CPU 路径每帧 resetDrawMetaToOriginal
        // 把 m_draw_meta_infos 重置为该值，避免单调污染。
        std::vector<DrawMetaInfo>           m_draw_meta_infos_template;
        // CPU 路径专用 host shadow：可见实例 id 列表（写入 set 1 / binding eVisibleInstances）。
        // 大小 = m_total_instance_count；按槽位写入而非顺序追加（见 writeVisibleInstanceAt 注释）。
        std::vector<uint32_t>               m_visible_instances;
        // 该帧实际可见的总数，写入 visible_ssbo 的 head 字段（不是 vector size）。
        uint32_t                            m_visible_total_count = 0;

        // 当前场景档位与导出的 instance 计数。
        SceneScaleConfig m_scale_config {};
        eScene           m_current_scene = eScene::eA;
        uint32_t         m_current_instance_per_mesh = 0u;
        uint32_t         m_total_instance_count      = 0u;
        uint32_t         m_object_count              = 0u;

        bool m_created = false;
    };

}  // namespace lcf::benchmark
