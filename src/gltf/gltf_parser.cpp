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

    glm::mat4 GltfParser::parseTransform(const tinygltf::Node &node) const
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

    std::vector<uint16_t> GltfParser::parseIndices(const tinygltf::Primitive &primitive, const tinygltf::Model &asset, const uint16_t &offset) const
    {
        if (primitive.indices < 0)
        {
            throw std::runtime_error("Indices accessor not found");
        }

        const tinygltf::Accessor &accessor = asset.accessors[primitive.indices];
        const tinygltf::BufferView &bufferView = asset.bufferViews[accessor.bufferView];
        const tinygltf::Buffer &buffer = asset.buffers[bufferView.buffer];

        std::vector<uint16_t> indices;
        indices.reserve(accessor.count);

        const uint8_t *data = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
        size_t stride = accessor.ByteStride(bufferView);
        if (stride == 0)
            stride = tinygltf::GetComponentSizeInBytes(accessor.componentType);

        try
        {
            for (size_t i = 0; i < accessor.count; i++)
            {
                uint16_t index = 0;
                switch (accessor.componentType)
                {
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    index = *reinterpret_cast<const uint16_t *>(data + i * stride);
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                    index = *reinterpret_cast<const uint8_t *>(data + i * stride);
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    index = static_cast<uint16_t>(*reinterpret_cast<const uint32_t *>(data + i * stride));
                    break;
                default:
                    throw std::runtime_error("Unsupported index component type");
                }
                indices.push_back(index + offset);
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error parsing indices: " << e.what() << std::endl;
        }

        return indices;
    }

    template <typename T>
    void getAttributeData(const tinygltf::Model &asset, int accessorIndex, std::vector<T> &outData)
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

    std::vector<VertexInput> GltfParser::parseVertexInputs(const tinygltf::Primitive &primitive, const tinygltf::Model &asset) const
    {
        auto positionIt = primitive.attributes.find("POSITION");
        if (positionIt == primitive.attributes.end())
        {
            throw std::runtime_error("Position attribute not found");
        }

        const tinygltf::Accessor &posAccessor = asset.accessors[positionIt->second];
        std::vector<VertexInput> vertex_inputs(posAccessor.count);

        try
        {
            // Position (required)
            std::vector<glm::vec3> positions;
            getAttributeData(asset, positionIt->second, positions);
            for (size_t i = 0; i < positions.size(); i++)
            {
                vertex_inputs[i].position = positions[i];
            }

            // Color
            auto colorIt = primitive.attributes.find("COLOR_0");
            if (colorIt != primitive.attributes.end())
            {
                std::vector<glm::vec4> colors;
                getAttributeData(asset, colorIt->second, colors);
                for (size_t i = 0; i < colors.size(); i++)
                {
                    vertex_inputs[i].color = colors[i];
                }
            }

            // Normal
            auto normalIt = primitive.attributes.find("NORMAL");
            if (normalIt != primitive.attributes.end())
            {
                std::vector<glm::vec3> normals;
                getAttributeData(asset, normalIt->second, normals);
                for (size_t i = 0; i < normals.size(); i++)
                {
                    vertex_inputs[i].normal = normals[i];
                }
            }

            // Tangent
            auto tangentIt = primitive.attributes.find("TANGENT");
            if (tangentIt != primitive.attributes.end())
            {
                std::vector<glm::vec4> tangents;
                getAttributeData(asset, tangentIt->second, tangents);
                for (size_t i = 0; i < tangents.size(); i++)
                {
                    vertex_inputs[i].tangent = tangents[i];
                }
            }

            // UV0
            auto uv0It = primitive.attributes.find("TEXCOORD_0");
            if (uv0It != primitive.attributes.end())
            {
                std::vector<glm::vec2> uvs;
                getAttributeData(asset, uv0It->second, uvs);
                for (size_t i = 0; i < uvs.size(); i++)
                {
                    vertex_inputs[i].uv0 = uvs[i];
                }
            }

            // UV1
            auto uv1It = primitive.attributes.find("TEXCOORD_1");
            if (uv1It != primitive.attributes.end())
            {
                std::vector<glm::vec2> uvs;
                getAttributeData(asset, uv1It->second, uvs);
                for (size_t i = 0; i < uvs.size(); i++)
                {
                    vertex_inputs[i].uv1 = uvs[i];
                }
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error parsing vertex attributes: " << e.what() << std::endl;
        }

        return vertex_inputs;
    }

    uint16_t GltfParser::parseMaterialIndex(const tinygltf::Primitive &primitive) const
    {
        if (primitive.material < 0)
        {
            throw std::runtime_error("Material index not found");
        }
        return static_cast<uint16_t>(primitive.material);
    }

    std::vector<PerMeshData> GltfParser::BuildMeshList(const tinygltf::Model &asset) const
    {
        std::vector<PerMeshData> meshes;
        meshes.reserve(asset.meshes.size());

        // collect all node transforms for each mesh
        std::vector<std::vector<glm::mat4>> meshTransforms(asset.meshes.size());
        for (const auto &node : asset.nodes)
        {
            if (node.mesh >= 0)
            {
                meshTransforms[node.mesh].push_back(parseTransform(node));
            }
        }

        // build mesh list
        for (size_t meshIndex = 0; meshIndex < asset.meshes.size(); ++meshIndex)
        {
            const auto &srcMesh = asset.meshes[meshIndex];
            const auto &transforms = meshTransforms[meshIndex];

            // skip meshes that are not referenced by any nodes
            if (transforms.empty())
            {
                continue;
            }

            uint16_t start_index = 0;
            uint16_t start_vertex = 0;
            PerMeshData destMesh;
            destMesh.name = srcMesh.name;
            if (destMesh.name.empty())
            {
                destMesh.name = "Mesh_" + std::to_string(meshIndex);
            }
            destMesh.primitives.reserve(srcMesh.primitives.size() * transforms.size());

            for (const auto &primitive : srcMesh.primitives)
            {
                // parse indices, vertex inputs and material index
                auto indices_result = parseIndices(primitive, asset, start_vertex);
                auto vertexInputs_result = parseVertexInputs(primitive, asset);
                auto materialIndex = parseMaterialIndex(primitive);

                // count current vertex and index count
                uint16_t indexCount = indices_result.size();
                uint16_t vertexCount = vertexInputs_result.size();

                // build draw call data
                for (const auto &transform : transforms)
                {
                    destMesh.primitives.push_back({.transform = transform,
                                                   .indices = std::move(indices_result),
                                                   .vertex_inputs = std::move(vertexInputs_result),
                                                   .material_index = materialIndex,
                                                   .first_index = start_index,
                                                   .index_count = indexCount,
                                                   .first_vertex = start_vertex,
                                                   .vertex_count = vertexCount});
                }

                // update first index and first vertex
                start_index += indexCount;
                start_vertex += vertexCount;
            }
            meshes.push_back(std::move(destMesh));
        }
        return meshes;
    }

    std::vector<PerDrawCallData> GltfParser::BuildDrawCallDataList(const tinygltf::Model &asset) const
    {
        std::vector<PerDrawCallData> drawCalls;

        // Process each node that has a mesh
        for (const auto &node : asset.nodes)
        {
            if (node.mesh < 0)
                continue;

            const auto &mesh = asset.meshes[node.mesh];
            auto transform = parseTransform(node);

            uint16_t start_index = 0;
            uint16_t start_vertex = 0;

            // Process each primitive in the mesh
            for (const auto &primitive : mesh.primitives)
            {
                // parse indices, vertex inputs and material index
                auto indices_result = parseIndices(primitive, asset, start_vertex);
                auto vertexInputs_result = parseVertexInputs(primitive, asset);
                auto material_index = parseMaterialIndex(primitive);

                // count current vertex and index count
                uint16_t indexCount = indices_result.size();
                uint16_t vertexCount = vertexInputs_result.size();

                // construct draw call data
                drawCalls.push_back({.transform = transform,
                                     .indices = std::move(indices_result),
                                     .vertex_inputs = std::move(vertexInputs_result),
                                     .material_index = material_index,
                                     .first_index = start_index,
                                     .index_count = indexCount,
                                     .first_vertex = start_vertex,
                                     .vertex_count = vertexCount});

                // update first index and first vertex
                start_index += indexCount;
                start_vertex += vertexCount;
            }
        }

        return drawCalls;
    }

} // namespace gltf
