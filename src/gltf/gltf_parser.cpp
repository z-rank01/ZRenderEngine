#include "gltf_parser.h"
#include <iostream>

namespace gltf
{

    std::vector<PerMeshData> GltfParser::operator()(const tinygltf::Model &asset, RequestMeshList) const
    {
        return BuildMeshList(asset);
    }

    std::vector<PerDrawCallData> GltfParser::operator()(const tinygltf::Model &asset, RequestDrawCallList) const
    {
        return BuildDrawCallDataList(asset);
    }

    glm::mat4 GltfParser::ParseTransform(const tinygltf::Node &node) const
    {
        if (!node.matrix.empty())
        {
            // 直接使用矩阵
            return glm::mat4(
                node.matrix[0], node.matrix[1], node.matrix[2], node.matrix[3],
                node.matrix[4], node.matrix[5], node.matrix[6], node.matrix[7],
                node.matrix[8], node.matrix[9], node.matrix[10], node.matrix[11],
                node.matrix[12], node.matrix[13], node.matrix[14], node.matrix[15]);
        }
        else
        {
            // 使用 TRS
            glm::mat4 transMat = node.translation.empty() ? glm::mat4(1.0f) : glm::translate(glm::mat4(1.0f), glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
            glm::mat4 rotMat = node.rotation.empty() ? glm::mat4(1.0f) : glm::mat4_cast(glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]));
            glm::mat4 scaleMat = node.scale.empty() ? glm::mat4(1.0f) : glm::scale(glm::mat4(1.0f), glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
            return transMat * rotMat * scaleMat;
        }
    }

    std::vector<uint32_t> GltfParser::ParseIndices(const tinygltf::Primitive &primitive, const tinygltf::Model &asset, const uint32_t &vertex_offset) const
    {
        if (primitive.indices < 0)
        {
            throw std::runtime_error("Indices accessor not found");
        }

        const tinygltf::Accessor &accessor = asset.accessors[primitive.indices];
        const tinygltf::BufferView &bufferView = asset.bufferViews[accessor.bufferView];
        const tinygltf::Buffer &buffer = asset.buffers[bufferView.buffer];

        std::vector<uint32_t> indices;
        indices.reserve(accessor.count);

        const uint8_t *data = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
        size_t stride = accessor.ByteStride(bufferView);
        if (stride == 0)
            stride = tinygltf::GetComponentSizeInBytes(accessor.componentType);

        try
        {
            for (size_t i = 0; i < accessor.count; i++)
            {
                uint32_t index = 0;
                switch (accessor.componentType)
                {
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    index = static_cast<uint32_t>(*reinterpret_cast<const uint16_t *>(data + i * stride));
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                    index = static_cast<uint32_t>(*reinterpret_cast<const uint8_t *>(data + i * stride));
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    index = *reinterpret_cast<const uint32_t *>(data + i * stride);
                    break;
                default:
                    throw std::runtime_error("Unsupported index component type");
                }
                indices.push_back(index + vertex_offset);
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error parsing indices: " << e.what() << std::endl;
        }

        return indices;
    }

    template <typename T>
    void GetAttributeData(const tinygltf::Model &asset, int accessorIndex, std::vector<T> &outData)
    {
        const tinygltf::Accessor &accessor = asset.accessors[accessorIndex];
        const tinygltf::BufferView &bufferView = asset.bufferViews[accessor.bufferView];
        const tinygltf::Buffer &buffer = asset.buffers[bufferView.buffer];

        const uint8_t *data = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
        size_t stride = accessor.ByteStride(bufferView);
        if (stride == 0)
            stride = sizeof(T);

        outData.resize(accessor.count);
        memcpy(outData.data(), data, accessor.count * stride);
    }

    std::vector<Vertex> GltfParser::ParseVertexInputs(const tinygltf::Primitive &primitive, const tinygltf::Model &asset) const
    {
        auto positionIt = primitive.attributes.find("POSITION");
        if (positionIt == primitive.attributes.end())
        {
            throw std::runtime_error("Position attribute not found");
        }

        const tinygltf::Accessor &posAccessor = asset.accessors[positionIt->second];
        std::vector<Vertex> vertices(posAccessor.count);

        try
        {
            // Position (required)
            std::vector<glm::vec3> positions;
            GetAttributeData(asset, positionIt->second, positions);
            for (size_t i = 0; i < positions.size(); i++)
            {
                vertices[i].position = positions[i];
            }

            // Color
            auto colorIt = primitive.attributes.find("COLOR_0");
            if (colorIt != primitive.attributes.end())
            {
                std::vector<glm::vec4> colors;
                GetAttributeData(asset, colorIt->second, colors);
                for (size_t i = 0; i < colors.size(); i++)
                {
                    vertices[i].color = colors[i];
                }
            }

            // Normal
            auto normalIt = primitive.attributes.find("NORMAL");
            if (normalIt != primitive.attributes.end())
            {
                std::vector<glm::vec3> normals;
                GetAttributeData(asset, normalIt->second, normals);
                for (size_t i = 0; i < normals.size(); i++)
                {
                    vertices[i].normal = normals[i];
                }
            }

            // Tangent
            auto tangentIt = primitive.attributes.find("TANGENT");
            if (tangentIt != primitive.attributes.end())
            {
                std::vector<glm::vec4> tangents;
                GetAttributeData(asset, tangentIt->second, tangents);
                for (size_t i = 0; i < tangents.size(); i++)
                {
                    vertices[i].tangent = tangents[i];
                }
            }

            // UV0
            auto uv0It = primitive.attributes.find("TEXCOORD_0");
            if (uv0It != primitive.attributes.end())
            {
                std::vector<glm::vec2> uvs;
                GetAttributeData(asset, uv0It->second, uvs);
                for (size_t i = 0; i < uvs.size(); i++)
                {
                    vertices[i].uv0 = uvs[i];
                }
            }

            // UV1
            auto uv1It = primitive.attributes.find("TEXCOORD_1");
            if (uv1It != primitive.attributes.end())
            {
                std::vector<glm::vec2> uvs;
                GetAttributeData(asset, uv1It->second, uvs);
                for (size_t i = 0; i < uvs.size(); i++)
                {
                    vertices[i].uv1 = uvs[i];
                }
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error parsing vertex attributes: " << e.what() << std::endl;
        }

        return vertices;
    }

    uint32_t GltfParser::ParseMaterialIndex(const tinygltf::Primitive &primitive) const
    {
        if (primitive.material < 0)
        {
            throw std::runtime_error("Material index not found");
        }
        return static_cast<uint32_t>(primitive.material);
    }

    std::vector<PerMeshData> GltfParser::BuildMeshList(const tinygltf::Model &asset) const
    {
        std::vector<PerMeshData> meshes;
        meshes.reserve(asset.meshes.size());

        // collect all node transforms for each mesh
        std::vector<std::vector<glm::mat4>> mesh_transforms(asset.meshes.size());
        for (const auto &node : asset.nodes)
        {
            if (node.mesh >= 0)
            {
                mesh_transforms[node.mesh].push_back(ParseTransform(node));
            }
        }

        // 添加全局顶点和索引偏移量计数器，在所有 mesh 之间累积
        uint32_t global_vertex_offset = 0;
        uint32_t global_index_offset = 0;

        // build mesh list
        for (size_t mesh_index = 0; mesh_index < asset.meshes.size(); ++mesh_index)
        {
            const auto &src_mesh = asset.meshes[mesh_index];
            const auto &transforms = mesh_transforms[mesh_index];

            // skip meshes that are not referenced by any nodes
            if (transforms.empty())
            {
                continue;
            }

            uint32_t mesh_vertex_offset = 0;
            PerMeshData dest_mesh;
            dest_mesh.name = src_mesh.name;
            if (dest_mesh.name.empty())
            {
                dest_mesh.name = "Mesh_" + std::to_string(mesh_index);
            }
            dest_mesh.primitives.reserve(src_mesh.primitives.size() * transforms.size());

            for (const auto &primitive : src_mesh.primitives)
            {
                // 为每个primitive计算全局顶点偏移量 = 全局偏移量 + 当前mesh内的偏移量
                uint32_t total_vertex_offset = global_vertex_offset + mesh_vertex_offset;

                // ParseIndices 已经处理了索引的顶点偏移量，它将每个原始索引值加上了 total_vertex_offset
                auto indices_result = ParseIndices(primitive, asset, total_vertex_offset);
                auto vertex_inputs_result = ParseVertexInputs(primitive, asset);
                auto material_index = ParseMaterialIndex(primitive);

                // count current vertex and index count
                uint32_t index_count = static_cast<uint32_t>(indices_result.size());
                uint32_t vertex_count = static_cast<uint32_t>(vertex_inputs_result.size());

                // build draw call data
                for (const auto &transform : transforms)
                {
                    dest_mesh.primitives.push_back({
                        .transform = transform,
                        .indices = std::move(indices_result),
                        .vertices = std::move(vertex_inputs_result),
                        .material_index = material_index,
                        // 注意：first_index 是在索引缓冲区中的索引位置，而不是顶点偏移量
                        // 这里使用 global_index_offset 是正确的，表示这个图元在全局索引数组中的起始位置
                        .first_index = global_index_offset,
                        .index_count = index_count,
                        // first_vertex 表示此图元的顶点在全局顶点数组中的起始位置
                        // 使用 total_vertex_offset 也是正确的
                        .first_vertex = total_vertex_offset,
                        .vertex_count = vertex_count
                    });
                }

                // update local vertex offset for next primitive in this mesh
                mesh_vertex_offset += vertex_count;
                
                // 更新全局索引偏移量，为下一个primitive准备
                global_index_offset += index_count;
            }
            
            // 更新全局顶点偏移量，为下一个mesh做准备
            global_vertex_offset += mesh_vertex_offset;
            
            meshes.push_back(std::move(dest_mesh));
        }
        return meshes;
    }

    std::vector<PerDrawCallData> GltfParser::BuildDrawCallDataList(const tinygltf::Model &asset) const
    {
        std::vector<PerDrawCallData> draw_calls;

        // 添加全局顶点偏移量和索引偏移量计数器
        uint32_t global_vertex_offset = 0;
        uint32_t global_index_offset = 0;

        // Process each node that has a mesh
        for (const auto &node : asset.nodes)
        {
            if (node.mesh < 0)
                continue;

            const auto &mesh = asset.meshes[node.mesh];
            auto transform = ParseTransform(node);

            uint32_t mesh_vertex_offset = 0;

            // Process each primitive in the mesh
            for (const auto &primitive : mesh.primitives)
            {
                // 为每个primitive计算全局顶点偏移量 = 全局偏移量 + 当前mesh内的偏移量
                uint32_t total_vertex_offset = global_vertex_offset + mesh_vertex_offset;

                // parse indices with total offset, vertex_inputs and material index
                auto indices_result = ParseIndices(primitive, asset, total_vertex_offset);
                auto vertex_inputs_result = ParseVertexInputs(primitive, asset);
                auto material_index = ParseMaterialIndex(primitive);

                // count current vertex and index count
                uint32_t index_count = static_cast<uint32_t>(indices_result.size());
                uint32_t vertex_count = static_cast<uint32_t>(vertex_inputs_result.size());

                // construct draw call data
                draw_calls.push_back({
                    .transform = transform,
                    .indices = std::move(indices_result),
                    .vertices = std::move(vertex_inputs_result),
                    .material_index = material_index,
                    .first_index = global_index_offset,  // 使用全局索引偏移量
                    .index_count = index_count,
                    .first_vertex = total_vertex_offset,  // 使用全局顶点偏移量
                    .vertex_count = vertex_count
                });

                // update local vertex offset for the next primitive
                mesh_vertex_offset += vertex_count;
                
                // 更新全局索引偏移量，为下一个primitive做准备
                global_index_offset += index_count;
            }

            // 更新全局顶点偏移量，为下一个mesh做准备
            global_vertex_offset += mesh_vertex_offset;
        }

        return draw_calls;
    }

} // namespace gltf
