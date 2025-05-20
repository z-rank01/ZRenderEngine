#include "vulkan_engine.h"
#include "utility/config_reader.h"
#include "utility/logger.h"
#include "gltf/gltf_loader.h"
#include "gltf/gltf_parser.h"
#include "gltf/gltf_converter.h"
#include <vma/vk_mem_alloc.h>

#include <iostream>

int main()
{
    std::cout << "Hello, World!" << '\n';
    std::cout << "This is a Vulkan Engine" << '\n';

    // read gltf file
    auto loader = gltf::GltfLoader();
    auto asset = loader("E:\\Assets\\Sponza\\SponzaBase\\NewSponza_Main_glTF_003.gltf");

    // parse gltf file
    gltf::GltfParser parser;
    const auto &mesh_list = parser(asset, gltf::RequestMeshList{});
    const auto &draw_call_data_list = parser(asset, gltf::RequestDrawCallList{});

    // per material data
    std::unordered_set<uint32_t> material_indices;
    std::vector<std::vector<uint32_t>> indices_per_material;
    std::vector<std::vector<gltf::VertexInput>> vertices_per_material;
    
    // collect all indices of materials
    std::transform(draw_call_data_list.begin(), draw_call_data_list.end(),
                  std::inserter(material_indices, material_indices.begin()),
                  [](const auto &draw_call_data) { return draw_call_data.material_index; });

    // generate per material data
    indices_per_material.resize(material_indices.size());
    vertices_per_material.resize(material_indices.size());
    gltf::DrawCalls2Indices draw_calls2indices;
    gltf::DrawCalls2Vertices draw_calls2vertices;
    std::for_each(material_indices.begin(), material_indices.end(), [&](const auto &material_index)
    {
        // filter draw call data by material index
        std::vector<gltf::PerDrawCallData> draw_call_data_per_material;
        std::copy_if(draw_call_data_list.begin(), draw_call_data_list.end(), std::back_inserter(draw_call_data_per_material), [&](const auto &draw_call_data)
        {
            return draw_call_data.material_index == material_index;
        });

        // generate indices and vertices of current material
        indices_per_material[material_index] = draw_calls2indices(draw_call_data_per_material);
        vertices_per_material[material_index] = draw_calls2vertices(draw_call_data_per_material);
    });

    // window config
    SWindowConfig window_config;
    const auto window_width = 800;
    const auto window_height = 600;
    const auto* window_name = "Vulkan Engine";

    window_config.width = window_width;
    window_config.height = window_height;
    window_config.title = window_name;

    // general config
    ConfigReader config_reader("E:\\Projects\\ZRenderGraph\\config\\win64\\app_config.json");
    SGeneralConfig general_config;
    if (!config_reader.TryParseGeneralConfig(general_config))
    {
        Logger::LogError("Failed to parse general config");
        return -1;
    }

    // engine config
    SEngineConfig config;
    config.window_config = window_config;
    config.general_config = general_config;
    config.frame_count = 3;
    config.use_validation_layers = true;
    

    // main loop
    VulkanEngine engine(config);
    engine.GetVertexIndexData(indices_per_material[0], vertices_per_material[0]);
    engine.Initialize();
    engine.Run();

    std::cout << "Goodbye" << '\n';

    return 0;
}
