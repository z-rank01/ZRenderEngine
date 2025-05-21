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
    // auto asset = loader("E:\\Assets\\Sponza\\SponzaCurtains\\NewSponza_Curtains_glTF.gltf");

    // parse gltf file
    gltf::GltfParser parser;
    auto mesh_list = parser(asset, gltf::RequestMeshList{});
    // Get a mutable copy of draw_call_data_list to transform vertex positions
    auto mutable_draw_call_data_list = parser(asset, gltf::RequestDrawCallList{});

    // Transform vertex positions using the draw call's transform matrix (functional expression)
    std::for_each(
        mesh_list.begin(), 
        mesh_list.end(),
        [](gltf::PerMeshData& mesh) 
        {
            // 遍历每个 primitive
            std::for_each(
                mesh.primitives.begin(),
                mesh.primitives.end(),
                [&](gltf::PerDrawCallData& primitive)
                {
                    const glm::mat4& transform = primitive.transform;
                    std::for_each(
                        primitive.vertex_inputs.begin(), 
                        primitive.vertex_inputs.end(),
                        [&](gltf::VertexInput& vertex_input) 
                        {
                            glm::vec4 transformed_position = transform * glm::vec4(vertex_input.position, 1.0f);
                            vertex_input.position = glm::vec3(transformed_position);
                        });
                });
        });

    // collect all indices
    std::vector<uint16_t> indices;
    std::vector<gltf::VertexInput> vertices;
    // reserve memory
    auto index_capacity = std::accumulate(mutable_draw_call_data_list.begin(), mutable_draw_call_data_list.end(), 0,
                                    [&](const auto &sum, const auto &draw_call_data)
                                    {
                                        return sum + draw_call_data.indices.size();
                                    });
    auto vertex_capacity = std::accumulate(mutable_draw_call_data_list.begin(), mutable_draw_call_data_list.end(), 0,
                                    [&](const auto &sum, const auto &draw_call_data)
                                    {
                                        return sum + draw_call_data.vertex_inputs.size();
                                    });
    indices.reserve(index_capacity);
    vertices.reserve(vertex_capacity);

    // collect all indices and vertices
    for (const auto& draw_call_data : mutable_draw_call_data_list) {
        indices.insert(indices.end(), draw_call_data.indices.begin(), draw_call_data.indices.end());
        vertices.insert(vertices.end(), draw_call_data.vertex_inputs.begin(), draw_call_data.vertex_inputs.end());
    }

    // // per material data
    // std::unordered_set<uint32_t> material_indices;
    // std::vector<std::vector<uint32_t>> indices_per_material;
    // std::vector<std::vector<gltf::VertexInput>> vertices_per_material;
    
    // // collect all indices of materials
    // std::transform(draw_call_data_list.begin(), draw_call_data_list.end(),
    //               std::inserter(material_indices, material_indices.begin()),
    //               [](const auto &draw_call_data) { return draw_call_data.material_index; });

    // // generate per material data
    // indices_per_material.resize(material_indices.size());
    // vertices_per_material.resize(material_indices.size());
    // gltf::DrawCalls2Indices draw_calls2indices;
    // gltf::DrawCalls2Vertices draw_calls2vertices;
    // std::for_each(material_indices.begin(), material_indices.end(), [&](const auto &material_index)
    // {
    //     // filter draw call data by material index
    //     std::vector<gltf::PerDrawCallData> draw_call_data_per_material;
    //     std::copy_if(draw_call_data_list.begin(), draw_call_data_list.end(), std::back_inserter(draw_call_data_per_material), [&](const auto &draw_call_data)
    //     {
    //         return draw_call_data.material_index == material_index;
    //     });

    //     // generate indices and vertices of current material
    //     indices_per_material[material_index] = draw_calls2indices(draw_call_data_per_material);
    //     vertices_per_material[material_index] = draw_calls2vertices(draw_call_data_per_material);
    // });

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
    engine.GetVertexIndexData(mutable_draw_call_data_list, indices, vertices);
    engine.GetMeshList(mesh_list);
    engine.Initialize();
    engine.Run();

    std::cout << "Goodbye" << '\n';

    return 0;
}
