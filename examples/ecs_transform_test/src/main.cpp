#include "Registry.h"
#include "Entity.h"
#include "TransformSystem.h"
#include "log.h"
#include <chrono>
#include <vector>
#include <algorithm>
#include <numeric>
#include <random>
#include "OOPTransform.h"
#include "TransformSystem2.h"


void performance_test(int num_nodes, float update_ratio, float attach_probability)
{
    int num_updates = static_cast<int>(num_nodes * update_ratio);
    std::vector<size_t> update_indices(num_updates);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, num_nodes - 1);

    for (int i = 0; i < num_updates; ++i) {
        update_indices[i] = dis(gen);
    }
    lcf::Registry registry_dfs, registry_oop;
    lcf::TransformSystem transform_system_dfs(registry_dfs);

    lcf::Registry registry_sep_oop;
    lcf::TransformSystem2 transform_system2(registry_sep_oop);
    
    std::vector<lcf::Entity> entities_dfs;
    std::vector<lcf::OOPTransform *> oop_transforms;
    std::vector<lcf::Entity> entities_oop;
    std::vector<lcf::Entity> entities_sep_oop;

    std::vector<lcf::Registry> junks;
    
    for (int i = 0; i < num_nodes; ++i) {
        entities_dfs.emplace_back(registry_dfs);
        entities_dfs[i].requireComponent<lcf::ENTTTransform>();
        entities_dfs[i].requireComponent<lcf::TransformHierarchy>();
        entities_dfs[i].requireComponent<lcf::TransformState>();
        for (int j = 0; j < 10; ++j) {
            junks.emplace_back();
        }
        oop_transforms.emplace_back(new lcf::OOPTransform);
        entities_oop.emplace_back(registry_oop);
        entities_oop[i].requireComponent<lcf::OOPTransform>();

        entities_sep_oop.emplace_back(registry_sep_oop);
        entities_sep_oop[i].requireComponent<lcf::OOPTransform2>();
        entities_sep_oop[i].requireComponent<lcf::OOPTransform2Hierarchy>();

    }
    std::vector<lcf::Registry>{}.swap(junks);
    
    for (int i = 1; i < num_nodes; ++i) {
        if (std::uniform_real_distribution<float>(0.0, 1.0)(gen) > attach_probability) { continue; }
        int parent_index = std::uniform_int_distribution<>(0, i - 1)(gen);
        if (parent_index == i) { continue; }
        // lcf_log_error("parent_index: {}, cur_index: {}", parent_index, i);
        entities_dfs[i].emitSignal<lcf::TransformHierarchyAttachSignalInfo>(entities_dfs[parent_index].getHandle());
        oop_transforms[i]->attachTo(oop_transforms[parent_index]);

        auto & child = entities_oop[i].getComponent<lcf::OOPTransform>();
        auto & parent = entities_oop[parent_index].getComponent<lcf::OOPTransform>();
        child.attachTo(&parent);

        entities_sep_oop[i].emitSignal<lcf::OOPTransform2AttachSignalInfo>(entities_sep_oop[parent_index].getHandle());
    }

    // // clear first dirty flag
    transform_system_dfs.update();
    for (int i = 0; i < num_nodes; ++i) {
        oop_transforms[i]->getWorldMatrix();
    }
    for (auto [entity, transform] : registry_oop.view<lcf::OOPTransform>().each()) {
        transform.getWorldMatrix();
    }
    transform_system2.update();
    

    for (size_t update_index : update_indices) {
        // lcf_log_info("update index: {}", update_index);
        lcf::Vector3D<float> offset(0.1, 0.2, 0.3);
        auto & entity_dfs = entities_dfs[update_index];
        entity_dfs.getComponent<lcf::ENTTTransform>().translateLocal(offset);
        entity_dfs.emitSignal<lcf::TransformUpdateSignalInfo>({}); 
        oop_transforms[update_index]->translateLocal(offset);

        auto & entity_oop = entities_oop[update_index];
        entity_oop.getComponent<lcf::OOPTransform>().translateLocal(offset);

        auto & entity_sep_oop = entities_sep_oop[update_index];

        entity_sep_oop.getComponent<lcf::OOPTransform2>().translateLocal(offset);
        entity_sep_oop.emitSignal<lcf::OOPTransform2UpdateSignalInfo>({});
    }

    auto start_dfs = std::chrono::high_resolution_clock::now();
    transform_system_dfs.update();
    auto end_dfs = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::micro> duration_dfs = end_dfs - start_dfs;

    std::vector<int> access_order(num_nodes);
    std::iota(access_order.begin(), access_order.end(), 0);
    std::ranges::shuffle(access_order, gen);

    auto start_oop = std::chrono::high_resolution_clock::now();
    for (int idx : access_order) {
        oop_transforms[idx]->getWorldMatrix();
    }
    auto end_oop = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::micro> duration_oop = end_oop - start_oop;

    auto start_oop_view = std::chrono::high_resolution_clock::now();
    for (auto [entity, transform] : registry_oop.view<lcf::OOPTransform>().each()) {
        transform.getWorldMatrix();
    }
    auto end_oop_view = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::micro> duration_oop_view = end_oop_view - start_oop_view;
    
    auto start_sep_oop = std::chrono::high_resolution_clock::now();
    transform_system2.update();
    auto end_sep_oop = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::micro> duration_sep_oop = end_sep_oop - start_sep_oop;
    
    lcf_log_info("Transform system dfs execution time: {:.4f} us", duration_dfs.count()); 
    lcf_log_info("OOP Transform execution time: {:.4f} us", duration_oop.count());
    lcf_log_info("OOP Transform view execution time: {:.4f} us", duration_oop_view.count());
    lcf_log_info("Separate OOP Transform execution time: {:.4f} us", duration_sep_oop.count());
    // lcf_log_info("update count dfs {}, oop {}", lcf::TransformSystem::s_update_count, lcf::OOPTransform::s_count);

    //check:
    bool all_matched = true;
    for (int i = 0; i < num_nodes; ++i) {
        const auto & mat_dfs = entities_dfs[i].getComponent<lcf::ENTTTransform>().getWorldMatrix();
        const auto & mat_oop = oop_transforms[i]->getPlainWorldMatrix();
        const auto & mat_oop_view = entities_oop[i].getComponent<lcf::OOPTransform>().getPlainWorldMatrix();
        const auto & mat_sep_oop = entities_sep_oop[i].getComponent<lcf::OOPTransform2>().getPlainWorldMatrix();
        // lcf_log_info("index{}\n{}\n{}", i, lcf::to_string(mat_sep_oop), lcf::to_string(mat_sep_oop_lowest));
        if (mat_dfs != mat_oop || mat_dfs != mat_oop_view || mat_dfs != mat_sep_oop) {
            all_matched = false;
            break;
        }
    }
    if (all_matched) {
        lcf_log_info("All transforms match!");
    } else {
        lcf_log_error("Transform results do not match!");
    }
    for (auto ptr : oop_transforms) { delete ptr; }
}

int main(int argc, char *argv[])
{
    lcf::Logger::init();
    lcf_log_warn("performance_test(10, 2f)");
    performance_test(10, 0.4f, 0.8);
    lcf_log_warn("performance_test(100, 0.4f)");
    performance_test(100, 0.4f, 0.8);
    lcf_log_warn("performance_test(500, 0.4f)"); 
    performance_test(500, 0.4f, 0.8);
    lcf_log_warn("performance_test(1000, 0.4f)");
    performance_test(1000, 0.4f, 0.8);
    lcf_log_warn("performance_test(2000, 0.4f)");
    performance_test(2000, 0.4f, 0.8);
    lcf_log_warn("performance_test(5000, 0.4f)");
    performance_test(5000, 0.4f, 0.8);
    lcf_log_warn("performance_test(10000, 0.4f)");
    performance_test(10000, 0.4f, 0.8);
    performance_test(50000, 0.4f, 0.8);

    return 0;
}