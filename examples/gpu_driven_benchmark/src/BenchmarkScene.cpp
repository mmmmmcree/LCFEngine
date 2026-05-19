// BenchmarkScene 的实现。
//
// 主要逻辑流以 RenderEngine/src/Vulkan/VulkanRenderer.cpp 为黄金参考，
// 该文件中已被验证零 bug。本文件做以下精简：
//   1) 仅保留主网格相关资源；不创建 skybox / sphere_to_cube 中间纹理（这些是
//      VulkanRenderer 的演示用途，与 benchmark 三路径对照无关）；
//   2) 不创建管线、framebuffer、命令缓冲——这些由各 renderer 自行管理；
//   3) 把 instance_data_list 的生成抽到 setSceneScale 中并延迟到 prepareFrame
//      统一 commit，使三种 renderer 在同一帧中观察到一致的 GPU 状态。

#include "BenchmarkScene.h"

#include <algorithm>
#include <cmath>
#include <ranges>
#include <utility>

#include <vulkan/vulkan.hpp>

#include "Frustum.h"
#include "Matrix.h"
#include "Quaternion.h"
#include "ResourceSystem.h"
#include "Transform.h"
#include "Vulkan/VulkanSampler.h"
#include "Vulkan/VulkanSamplerManager.h"
#include "Vulkan/VulkanTextureManager.h"
#include "Vulkan/ds/VulkanBindlessDescriptorSet.h"
#include "Vulkan/ds/VulkanDescriptorSetManager.h"
#include "Vulkan/vulkan_constants.h"
#include "Vulkan/vulkan_enums.h"
#include "Vulkan/vulkan_utililtie.h"
#include "bytes.h"
#include "common/glsl_type_traits.h"
#include "ecs/Registry.h"
#include "log.h"
#include "render_assets/ModelLoader.h"
#include "render_assets/Texture2D.h"

using lcf::BoundingSphere;
using lcf::Model;
using lcf::ModelLoader;
using lcf::Texture2D;
using lcf::VertexAttribute;
using lcf::VertexAttributeFlags;
using lcf::ShadingModel;
using lcf::generate_interleaved_segments;
using lcf::view_geometries;
using lcf::view_materials;
using lcf::get_textures;
using lcf::render::GPUBufferPattern;
using lcf::render::GPUBufferUsage;
using lcf::render::SamplerPreset;
using lcf::render::VulkanAttachment;
using lcf::render::VulkanBindlessTextureIdTable;
using lcf::render::VulkanImageObject;
using namespace lcf::render::vkenums;

namespace lcf::benchmark {

    namespace {
        // 与 VulkanRenderer.cpp::create() 中初始 m_per_renderable_ssbo_group.emplace 顺序一致：
        //   [0] DrawMetaInfos（含 1 字 uint draw_count head；usage = eIndirect）
        //   [1] ObjectData
        //   [2] VisibleInstances（含 1 字 uint instance_count head）
        //   [3] InstanceData
        //   [4] BoundingSphere
        // 容量按"最大档位 × mesh 数"计算，留 4 倍冗余便于动态切档。
        constexpr uint32_t k_capacity_safety_factor = 2u;
        constexpr uint32_t k_assumed_max_meshes     = 64u;  // 两个 gltf 合并约 30 mesh，留余量。

        uint32_t compute_max_instance_per_mesh(const SceneScaleConfig & cfg) noexcept
        {
            return *std::ranges::max_element(cfg.instance_per_mesh);
        }
    }

    BenchmarkScene::~BenchmarkScene()
    {
        if (m_context_p) {
            // 显式等待 GPU 工作完成再让成员析构，避免 in-flight buffer 还在被 GPU 使用。
            m_context_p->getDevice().waitIdle();
        }
    }

    void BenchmarkScene::create(render::VulkanContext * context, ecs::Registry & registry)
    {
        if (m_created) { return; }
        m_context_p   = context;
        m_registry_p  = &registry;
        auto device    = context->getDevice();
        auto & resource_system = registry.ctx().get<ecs::ResourceSystem>();
        auto & ds_manager      = context->getDescriptorSetManager();
        lcf_log_info("[scene] entered create()");

        // 关键：与 VulkanRenderer.cpp:88 对位 — 必须先初始化 bindless set 的
        // frame_copies。否则 BindlessDescriptorSet::commitUpdate 内的
        // `m_current_index % m_frame_slots.size()` 会触发 STATUS_INTEGER_DIVIDE_BY_ZERO
        // (m_frame_slots 默认空)。
        // 用 3 与 RendererSwitcher 内的 frames-in-flight 保持一致。
        if (auto ec = ds_manager.initBindlessSets(3); ec) {
            lcf_log_error("[scene] initBindlessSets failed: {}", ec.message());
        }
        lcf_log_info("[scene] bindless sets initialized (frame_copies=3)");

        // PerView 描述符集 layout（仅 1 个 UBO binding，与 vkconstants::ds::k_per_view_bindings 一致）。
        m_per_view_descriptor_set_layout
            .setBindings(render::vkconstants::ds::k_per_view_bindings)
            .setIndex(std::to_underlying(DescriptorSetIndex::ePerView))
            .create(device, DescriptorSetStrategy::eIndividual);
        lcf_log_info("[scene] perview ds layout ok");

        // PerView UBO（动态写入）。
        m_per_view_uniform_buffer
            .setUsage(GPUBufferUsage::eUniform)
            .setPattern(GPUBufferPattern::eDynamic)
            .create(m_context_p, lcf::size_of_v<CameraData>);
        lcf_log_info("[scene] perview ubo ok");

        // 5 路 SSBO 容量按"最大档位"预留，避免运行时切换档位触发 resize。
        const uint32_t max_instance_per_mesh = compute_max_instance_per_mesh(m_scale_config);
        const uint32_t max_total_instances   = max_instance_per_mesh * k_assumed_max_meshes
                                                * k_capacity_safety_factor;
        const uint32_t max_objects           = k_assumed_max_meshes * k_capacity_safety_factor;
        lcf_log_info("[scene] capacities: max_instance_per_mesh={} max_total_instances={} max_objects={}",
                     max_instance_per_mesh, max_total_instances, max_objects);

        m_per_renderable_ssbo_group.create(m_context_p, GPUBufferPattern::eDynamic);
        m_per_renderable_ssbo_group.emplace(
            sizeof(uint32_t) + max_objects * sizeof(DrawMetaInfo),
            GPUBufferUsage::eIndirect);                        // [0] DrawMetaInfos
        m_per_renderable_ssbo_group.emplace(
            sizeof(uint64_t) + max_objects * sizeof(ObjectData),
            GPUBufferUsage::eShaderStorage);                   // [1] ObjectData
        m_per_renderable_ssbo_group.emplace(
            sizeof(uint32_t) + max_total_instances * sizeof(uint32_t),
            GPUBufferUsage::eShaderStorage);                   // [2] VisibleInstances
        m_per_renderable_ssbo_group.emplace(
            max_total_instances * sizeof(InstanceData),
            GPUBufferUsage::eShaderStorage);                   // [3] InstanceData
        m_per_renderable_ssbo_group.emplace(
            max_objects * sizeof(BoundingSphere<float>),
            GPUBufferUsage::eShaderStorage);                   // [4] BoundingSphere
        lcf_log_info("[scene] 5 ssbos emplaced");

        // 创建 PerView 描述符集并把 UBO 写进去。
        m_per_view_descriptor_set = ds_manager.createSet(m_per_view_descriptor_set_layout);
        this->writePerViewDescriptorSet();
        lcf_log_info("[scene] perview ds created");

        // 上传两个 gltf。注意 immediate_submit 在 transfer queue 上同步等待。
        auto upload = [&](const std::string & path) {
            lcf_log_info("[scene] uploading model: {}", path);
            auto load_result = ModelLoader{}.load(path);
            if (not load_result) {
                lcf_log_warn("BenchmarkScene: failed to load model: {}", path);
                return;
            }
            render::vkutils::immediate_submit(
                m_context_p,
                vk::QueueFlagBits::eTransfer,
                [&, this](render::VulkanCommandBufferObject & cmd) {
                    this->uploadModel(cmd, load_result.value(), resource_system);
                });
            lcf_log_info("[scene] uploaded model: {}", path);
        };
        upload("./assets/models/FlightHelmet/glTF/FlightHelmet.gltf");
        upload("./assets/models/DamagedHelmet/DamagedHelmet.gltf");

        // 提交完毕后写入 BindlessBuffer 与 BindlessTexture 描述符集。
        this->writeBindlessBufferDescriptorSet();
        lcf_log_info("[scene] bindless buffer ds written");
        this->writeBindlessTextureDescriptorSet();
        lcf_log_info("[scene] bindless texture ds written");

        // 计算 object_count 与 mesh_count 并初始化首个场景档位。
        m_object_count = static_cast<uint32_t>(m_material_params_list.size());
        lcf_log_info("[scene] object_count = {}", m_object_count);
        this->setSceneScale(eScene::eA);
        lcf_log_info("[scene] setSceneScale(eA) ok");

        m_created = true;
        lcf_log_info("BenchmarkScene created: object_count={}", m_object_count);
    }

    void BenchmarkScene::uploadModel(
        render::VulkanCommandBufferObject & cmd,
        lcf::Model & model,
        ecs::ResourceSystem & resource_system)
    {
        auto & mesh_pack = m_mesh_packs.emplace_back();
        for (const auto & geometry : model.getRenderPrimitives() | view_geometries) {
            auto & mesh = mesh_pack.meshes.emplace_back();
            mesh.create(
                m_context_p, cmd,
                generate_interleaved_segments<glsl::std140::enum_value_type_mapping_t>(
                    geometry,
                    VertexAttributeFlags::ePosition |
                    VertexAttributeFlags::eNormal |
                    VertexAttributeFlags::eTexCoord0 |
                    VertexAttributeFlags::eTangent),
                geometry.getIndices());
            BoundingSphere<float> sphere{
                geometry.getAttributes<VertexAttribute::ePosition>()};
            mesh.setBoundingSphere(sphere);
        }

        for (const auto & material : model.getRenderPrimitives() | view_materials) {
            auto & params_buffer = m_material_params_list.emplace_back();
            params_buffer.setUsage(GPUBufferUsage::eShaderStorage)
                         .setPattern(GPUBufferPattern::eStatic)
                         .create(m_context_p, 1);
            params_buffer.appendWriteSegments(
                generate_interleaved_segments<glsl::std140::enum_value_type_mapping_t>(
                    material, ShadingModel::eStandard));

            auto & texture_ids_buffer = m_material_texture_ids_list.emplace_back();
            texture_ids_buffer.setUsage(GPUBufferUsage::eShaderStorage)
                              .setPattern(GPUBufferPattern::eStatic)
                              .create(m_context_p, 1);

            for (const auto & texture_resource : get_textures(material, ShadingModel::eStandard)) {
                if (m_texture_id_map.contains(&texture_resource)) { continue; }
                VulkanImageObject image_obj;
                image_obj.setFormat(enum_cast<vk::Format>(texture_resource.getDecodeFormat()))
                         .setExtent({texture_resource.getWidth(), texture_resource.getHeight(), 1u})
                         .setUsage(vk::ImageUsageFlagBits::eSampled |
                                   vk::ImageUsageFlagBits::eTransferDst)
                         .create(m_context_p);
                image_obj.setData(cmd, texture_resource.getDataSpan());
                auto texture_re = resource_system.registerResource(std::move(image_obj));
                uint32_t texture_id = m_bindless_texture_id_table.registerTexture(
                    BindlessTextureBinding::eTexture2Ds, texture_re->getHandle());
                m_texture_id_map[&texture_resource] = texture_id;
                m_texture_re_list.emplace_back(texture_id, std::move(texture_re));
            }
            for (uint32_t offset = 0;
                 const auto & texture_resource : get_textures(material, ShadingModel::eStandard)) {
                texture_ids_buffer.addWriteSegment({
                    lcf::as_bytes_from_value(m_texture_id_map[&texture_resource]),
                    offset});
                offset += sizeof(uint32_t);
            }
            params_buffer.commit(cmd);
            texture_ids_buffer.commit(cmd);
        }
    }

    void BenchmarkScene::writePerViewDescriptorSet()
    {
        vk::DescriptorBufferInfo buffer_info;
        buffer_info.setBuffer(m_per_view_uniform_buffer.getHandle())
                   .setOffset(0)
                   .setRange(vk::WholeSize);
        m_per_view_descriptor_set
            .addDescriptorInfo(std::to_underlying(render::PerViewBindingPoints::eCamera),
                               buffer_info)
            .commitUpdate(m_context_p->getDevice());
    }

    void BenchmarkScene::writeBindlessBufferDescriptorSet()
    {
        lcf_log_info("[scene] writeBindlessBufferDescriptorSet: enter");
        auto & bindless_buffer_ds = m_context_p->getDescriptorSetManager().getBindlessBufferSet();
        lcf_log_info("[scene] writeBindlessBufferDescriptorSet: ds got");
        bindless_buffer_ds
            .addDescriptorInfo(
                std::to_underlying(BindlessBufferBinding::eObjectData),
                m_per_renderable_ssbo_group[std::to_underlying(BindlessBufferBinding::eObjectData)]
                    .generateBufferInfo())
            .addDescriptorInfo(
                std::to_underlying(BindlessBufferBinding::eDrawMetaInfos),
                m_per_renderable_ssbo_group[std::to_underlying(BindlessBufferBinding::eDrawMetaInfos)]
                    .generateBufferInfo())
            .addDescriptorInfo(
                std::to_underlying(BindlessBufferBinding::eInstanceData),
                m_per_renderable_ssbo_group[std::to_underlying(BindlessBufferBinding::eInstanceData)]
                    .generateBufferInfo())
            .addDescriptorInfo(
                std::to_underlying(BindlessBufferBinding::eVisibleInstances),
                m_per_renderable_ssbo_group[std::to_underlying(BindlessBufferBinding::eVisibleInstances)]
                    .generateBufferInfo())
            .addDescriptorInfo(
                std::to_underlying(BindlessBufferBinding::eBoundingVolume),
                m_per_renderable_ssbo_group[std::to_underlying(BindlessBufferBinding::eBoundingVolume)]
                    .generateBufferInfo());
        lcf_log_info("[scene] writeBindlessBufferDescriptorSet: addDescriptorInfo x5 ok");
        bindless_buffer_ds.commitUpdate(m_context_p->getDevice());
        lcf_log_info("[scene] writeBindlessBufferDescriptorSet: commitUpdate ok");
    }

    void BenchmarkScene::writeBindlessTextureDescriptorSet()
    {
        auto & ds_manager       = m_context_p->getDescriptorSetManager();
        auto & bindless_texture = ds_manager.getBindlessTextureSet();
        const auto & sampler_manager = m_context_p->getSamplerManager();

        for (const auto & [texture_id, texture_re] : m_texture_re_list) {
            vk::DescriptorImageInfo image_info;
            image_info.setImageLayout(*texture_re->getLayout())
                      .setImageView(texture_re->getDefaultView());
            bindless_texture.addDescriptorInfo(
                std::to_underlying(BindlessTextureBinding::eTexture2Ds),
                texture_id,
                image_info,
                texture_re.lease());
        }

        // 至少在 binding 0 / index 0 写一个 sampler，避免 fragment shader 访问空槽。
        vk::DescriptorImageInfo sampler_info;
        sampler_info.setSampler(sampler_manager.get(SamplerPreset::eColorMap));
        bindless_texture.addDescriptorInfo(
            std::to_underlying(BindlessTextureBinding::eSamplers), 0, sampler_info);

        bindless_texture.commitUpdate(m_context_p->getDevice());
    }

    void BenchmarkScene::setSceneScale(eScene scene)
    {
        m_current_scene             = scene;
        m_current_instance_per_mesh = m_scale_config.instance_per_mesh[std::to_underlying(scene)];

        // 物体级数据：每个 mesh 一个 ObjectData / BoundingSphere / DrawMetaInfo。
        m_object_data_list.clear();
        m_bounding_spheres_list.clear();
        m_draw_meta_infos.clear();
        m_object_data_list.reserve(m_object_count);
        m_bounding_spheres_list.reserve(m_object_count);
        m_draw_meta_infos.reserve(m_object_count);

        // 「同 pack 内 mesh 共享位置」策略（参考 VulkanRenderer.cpp:530 instance pool 复制）：
        //   - 每 pack 占 mesh_count(pack) * instance_per_mesh 个 instance pool slot
        //   - 同一 pack 内的 N 个 mesh 各自的 first_instance 指向 pool 中的不同段
        //     但这 N 段对应的位置数据完全相同（rebuildInstanceData 中负责复制）
        //   - 不同 pack 占用 cube 网格中的下一段空间点（pack_baseline 推进）
        //   - 位置点总数 = pack_count * instance_per_mesh（cube 阵列要排布的真实点数）
        uint32_t object_id = 0;
        uint32_t pool_cursor = 0;          // instance pool 当前写入位置
        for (const auto & pack : m_mesh_packs) {
            const uint32_t pack_mesh_count = static_cast<uint32_t>(pack.meshes.size());
            const uint32_t pack_pool_size  = pack_mesh_count * m_current_instance_per_mesh;
            for (uint32_t i = 0; i < pack_mesh_count; ++i) {
                const auto & mesh = pack.meshes[i];
                ObjectData od;
                od.m_vb_address = mesh.getVertexBufferAddress();
                od.m_ib_address = mesh.getIndexBufferAddress();
                if (object_id < m_material_params_list.size()) {
                    od.m_material_params_address      = m_material_params_list[object_id].getDeviceAddress();
                    od.m_material_texture_ids_address = m_material_texture_ids_list[object_id].getDeviceAddress();
                }
                m_object_data_list.emplace_back(od);
                m_bounding_spheres_list.emplace_back(mesh.getBoundingSphere());

                DrawMetaInfo dmi;
                dmi.m_vertex_count   = mesh.getIndexCount();
                dmi.m_instance_count = m_current_instance_per_mesh;
                dmi.m_first_vertex   = 0u;
                dmi.m_first_instance = pool_cursor + i * m_current_instance_per_mesh;
                dmi.m_object_id      = object_id;
                m_draw_meta_infos.emplace_back(dmi);

                ++object_id;
            }
            pool_cursor += pack_pool_size;
        }
        m_total_instance_count = pool_cursor;

        // 保存一份原始 draw_meta_infos 作为 CPU 路径每帧 reset 的模板。
        m_draw_meta_infos_template = m_draw_meta_infos;

        // 同时清掉跨档位切换时残留的 visible host shadow（避免新档位的实例数与旧 buffer 不匹配）。
        m_visible_instances.clear();
        m_visible_total_count = 0u;

        // 实例级数据：在三维立方体网格上分布（参考 cull-and-lod 类测试的经典布局），
        // 让 frustum cull / occlusion cull / LOD 的差异在 x/y/z 三个方向都能体现。
        this->rebuildInstanceData();
    }

    void BenchmarkScene::rebuildInstanceData()
    {
        m_instance_data_list.clear();
        m_instance_data_list.reserve(m_total_instance_count);

        // 真实位置点数 = pack_count * instance_per_mesh（同 pack 内 N mesh 共享位置）。
        const uint32_t pack_count = static_cast<uint32_t>(m_mesh_packs.size());
        const uint32_t unique_position_count = pack_count * m_current_instance_per_mesh;

        if (unique_position_count == 0u) {
            return;
        }

        // 在 3D 立方体网格上分布 unique_position_count 个真实位置，居中于原点。
        // side = ceil(cbrt(N))，索引 i → (x = i % side, y = (i / side) % side, z = i / (side*side))
        const uint32_t side = static_cast<uint32_t>(
            std::ceil(std::cbrt(static_cast<float>(unique_position_count))));
        const float spacing = 3.0f;
        const float origin_offset = -static_cast<float>(side - 1) * 0.5f * spacing;

        // 先生成 unique_position_count 个真实 transform。
        std::vector<InstanceData> unique_instances;
        unique_instances.reserve(unique_position_count);
        for (uint32_t i = 0; i < unique_position_count; ++i) {
            const uint32_t cx = i % side;
            const uint32_t cy = (i / side) % side;
            const uint32_t cz = i / (side * side);
            InstanceData inst;
            inst.m_transform.translateWorldX(origin_offset + static_cast<float>(cx) * spacing);
            inst.m_transform.translateWorldY(origin_offset + static_cast<float>(cy) * spacing);
            inst.m_transform.translateWorldZ(origin_offset + static_cast<float>(cz) * spacing);
            unique_instances.emplace_back(inst);
        }

        // 按 setSceneScale 中相同的 pack→mesh 顺序展开到 instance pool：
        //   每 pack 占 unique_position_count 中的连续 instance_per_mesh 段（pack i 对应 [i*ipm, (i+1)*ipm)）；
        //   该段在 pool 内被 mesh_count(pack) 次复制（同 pack 内 N mesh 共享同一组位置）。
        for (uint32_t pack_idx = 0; pack_idx < pack_count; ++pack_idx) {
            const uint32_t pack_mesh_count = static_cast<uint32_t>(m_mesh_packs[pack_idx].meshes.size());
            const uint32_t pos_begin = pack_idx * m_current_instance_per_mesh;
            for (uint32_t copy = 0; copy < pack_mesh_count; ++copy) {
                for (uint32_t k = 0; k < m_current_instance_per_mesh; ++k) {
                    m_instance_data_list.emplace_back(unique_instances[pos_begin + k]);
                }
            }
        }
    }

    void BenchmarkScene::setInstancePerMeshOverride(const SceneScaleConfig & config) noexcept
    {
        m_scale_config = config;
        // 若 create 已完成，立即按当前档位重建以反映 override。
        if (m_created) { this->setSceneScale(m_current_scene); }
    }

    void BenchmarkScene::updateCameraShadow(
        const ecs::Entity & camera,
        std::pair<uint32_t, uint32_t> render_extent)
    {
        auto [width, height] = render_extent;
        if (width == 0u or height == 0u) { return; }

        Matrix4x4<float> projection;
        projection.perspectiveRH_ZO(60.0f,
                                    static_cast<float>(width) / static_cast<float>(height),
                                    0.1f, 1000.0f);
        const auto & cam_view_inv = camera.getComponent<TransformInvertedWorldMatrix>();
        Matrix4x4<float> projection_view = projection * cam_view_inv.getMatrix();

        Frustum<float> frustum;
        frustum.update(projection_view);

        const auto & cam_transform = camera.getComponent<Transform>();
        const auto position = cam_transform.getTranslation();

        m_camera_data_shadow.m_projection      = projection;
        m_camera_data_shadow.m_view            = cam_view_inv.getMatrix();
        m_camera_data_shadow.m_projection_view = projection_view;
        m_camera_data_shadow.m_position        = Vector4D<float>{position.getX(), position.getY(), position.getZ(), 1.0f};
        m_camera_data_shadow.m_frustum         = frustum;
    }

    void BenchmarkScene::resetDrawMetaToOriginal()
    {
        m_draw_meta_infos = m_draw_meta_infos_template;
    }

    void BenchmarkScene::overrideDrawMetaInstanceCount(uint32_t draw_index, uint32_t new_count)
    {
        if (draw_index < m_draw_meta_infos.size()) {
            m_draw_meta_infos[draw_index].m_instance_count = new_count;
        }
    }

    void BenchmarkScene::resizeVisibleInstancesBuffer()
    {
        // 大小固定为 m_total_instance_count；按槽位写入。
        if (m_visible_instances.size() != m_total_instance_count) {
            m_visible_instances.assign(m_total_instance_count, 0u);
        } else {
            // 复用 buffer 但清掉上一帧残留（防止某 mesh 全剔除时槽位残留）。
            std::fill(m_visible_instances.begin(), m_visible_instances.end(), 0u);
        }
        m_visible_total_count = 0u;
    }

    void BenchmarkScene::writeVisibleInstanceAt(uint32_t slot, uint32_t instance_id)
    {
        if (slot < m_visible_instances.size()) {
            m_visible_instances[slot] = instance_id;
            ++m_visible_total_count;
        }
    }

    void BenchmarkScene::clearVisibleInstancesShadow() noexcept
    {
        m_visible_instances.clear();
        m_visible_total_count = 0u;
    }

    void BenchmarkScene::prepareFrameDataTransfer(
        render::VulkanCommandBufferObject & transfer_cmd,
        const ecs::Entity & camera,
        std::pair<uint32_t, uint32_t> render_extent)
    {
        // 1) 更新 PerView UBO 的 host shadow，再把每段写到 buffer 的 pending segments。
        // 注意：CPU 路径会在 prepareFrameDataTransfer 之前先调 updateCameraShadow + 剔除，
        // 此时 host shadow 已是最新；这里再调一次是幂等的（只读 camera ECS 数据）。
        this->updateCameraShadow(camera, render_extent);
        m_per_view_uniform_buffer
            .addWriteSegment({lcf::as_bytes_from_value(m_camera_data_shadow.m_projection),
                              offsetof(CameraData, m_projection)})
            .addWriteSegment({lcf::as_bytes_from_value(m_camera_data_shadow.m_view),
                              offsetof(CameraData, m_view)})
            .addWriteSegment({lcf::as_bytes_from_value(m_camera_data_shadow.m_projection_view),
                              offsetof(CameraData, m_projection_view)})
            .addWriteSegment({lcf::as_bytes_from_value(m_camera_data_shadow.m_position),
                              offsetof(CameraData, m_position)})
            .addWriteSegment({lcf::as_bytes_from_value(m_camera_data_shadow.m_frustum),
                              offsetof(CameraData, m_frustum)});

        // 2) 5 路 SSBO 的 host shadow（DrawMetaInfo 头部带 1 字 uint draw_count，
        //    与 GLSL 侧 layout(std430) 的 "uint draw_count; DrawMetaInfo[]" 对齐）。
        const uint32_t draw_count = static_cast<uint32_t>(m_draw_meta_infos.size());
        auto & draw_meta_ssbo  = m_per_renderable_ssbo_group[std::to_underlying(BindlessBufferBinding::eDrawMetaInfos)];
        auto & object_ssbo     = m_per_renderable_ssbo_group[std::to_underlying(BindlessBufferBinding::eObjectData)];
        auto & instance_ssbo   = m_per_renderable_ssbo_group[std::to_underlying(BindlessBufferBinding::eInstanceData)];
        auto & bounding_ssbo   = m_per_renderable_ssbo_group[std::to_underlying(BindlessBufferBinding::eBoundingVolume)];
        auto & visible_ssbo    = m_per_renderable_ssbo_group[std::to_underlying(BindlessBufferBinding::eVisibleInstances)];

        draw_meta_ssbo
            .addWriteSegment({lcf::as_bytes_from_value(draw_count), 0u})
            .addWriteSegment({lcf::as_bytes(m_draw_meta_infos), sizeof(uint32_t)});

        // ObjectData head: GLSL 侧用 uint64_t object_count，所以预留 8 字节。
        const uint64_t object_count64 = static_cast<uint64_t>(m_object_count);
        object_ssbo
            .addWriteSegment({lcf::as_bytes_from_value(object_count64), 0u})
            .addWriteSegment({lcf::as_bytes(m_object_data_list), sizeof(uint64_t)});

        instance_ssbo.addWriteSegment({lcf::as_bytes(m_instance_data_list), 0u});
        bounding_ssbo.addWriteSegment({lcf::as_bytes(m_bounding_spheres_list), 0u});

        // 3) 仅 CPU 路径会调 resizeVisibleInstancesBuffer；GPU 路径让 cull.comp 在 GPU 上写。
        // 用 m_visible_total_count（剔除真实总数）作为 head，而非 vector size（=
        // m_total_instance_count，含未填槽位）。
        if (not m_visible_instances.empty()) {
            visible_ssbo
                .addWriteSegment({lcf::as_bytes_from_value(m_visible_total_count), 0u})
                .addWriteSegment({lcf::as_bytes(m_visible_instances), sizeof(uint32_t)});
        }

        // 4) 真正的 commit（每路 buffer 内部按 dirty segments 触发一次 vkCmdCopyBuffer）。
        m_per_view_uniform_buffer.commit(transfer_cmd);
        m_per_renderable_ssbo_group.commitAll(transfer_cmd);
    }

    uint32_t BenchmarkScene::getVisibleInstancesBufferOffset() const noexcept
    {
        // 与 GLSL 端 "uint instance_count; uint visible_instance_ids[]" 对齐：
        // CPU 路径写入 instance_count（首字）后，instance_id 数组从 offset = 4 起。
        return static_cast<uint32_t>(sizeof(uint32_t));
    }

}  // namespace lcf::benchmark
