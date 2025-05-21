#ifndef GLTF_PARSER_H
#define GLTF_PARSER_H

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <algorithm>
#include <ranges>
#include <vector>
#include <string>
#include <numeric>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <variant>
#include <tuple>

#include "gltf_data.h"

namespace gltf
{
    // Define tag structs for overloading
    struct RequestMeshList {};
    struct RequestDrawCallList {};

    class GltfParser
    {
    public:
        GltfParser() = default;

        /// @brief Parse gltf file and return mesh list
        /// @param asset: gltf asset
        /// @param request: request type
        /// @return: mesh list
        std::vector<PerMeshData> operator()(const fastgltf::Asset &asset, RequestMeshList) const { return BuildMeshList(asset);  }

        /// @brief Parse gltf file and return draw call data list
        /// @param asset: gltf asset
        /// @param request: request type
        /// @return: draw call data list
        std::vector<PerDrawCallData> operator()(const fastgltf::Asset &asset, RequestDrawCallList) const { return BuildDrawCallDataList(asset); }

    private:
        glm::mat4                       parseTransform(const fastgltf::Node &node) const;
        std::vector<uint32_t>           parseIndices(const fastgltf::Primitive &primitive, const fastgltf::Asset &asset, const uint32_t &offset) const;
        std::vector<VertexInput>        parseVertexInputs(const fastgltf::Primitive &primitive, const fastgltf::Asset &asset) const;
        uint32_t                        parseMaterialIndex(const fastgltf::Primitive &primitive) const;

        std::vector<PerMeshData>        BuildMeshList(const fastgltf::Asset &asset) const;
        std::vector<PerDrawCallData>    BuildDrawCallDataList(const fastgltf::Asset &asset) const;

        template<typename T, typename Setter>
        void parseAttributeInternal(const fastgltf::Asset &asset, size_t accessorIndex, Setter setter) const;
    };

    glm::mat4 GltfParser::parseTransform(const fastgltf::Node &node) const
    {
        if (std::holds_alternative<fastgltf::math::fmat4x4>(node.transform)) {
            const auto& mat = std::get<fastgltf::math::fmat4x4>(node.transform);
            // 使用 toGLMVec4 转换
            return glm::mat4(
                glm::vec4(mat.row(0)[0], mat.row(0)[1], mat.row(0)[2], mat.row(0)[3]),
                glm::vec4(mat.row(1)[0], mat.row(1)[1], mat.row(1)[2], mat.row(1)[3]),
                glm::vec4(mat.row(2)[0], mat.row(2)[1], mat.row(2)[2], mat.row(2)[3]),
                glm::vec4(mat.row(3)[0], mat.row(3)[1], mat.row(3)[2], mat.row(3)[3])
            );
        } else {
            const auto &trs = std::get<fastgltf::TRS>(node.transform);
            glm::quat rotation(trs.rotation[3], trs.rotation[0], trs.rotation[1], trs.rotation[2]);
            glm::mat4 rotMat = glm::mat4_cast(rotation);
            glm::mat4 transMat = glm::translate(glm::mat4(1.0f), glm::vec3(trs.translation[0], trs.translation[1], trs.translation[2]));
            glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), glm::vec3(trs.scale[0], trs.scale[1], trs.scale[2]));
            return transMat * rotMat * scaleMat;
        }
    }

    std::vector<uint32_t> GltfParser::parseIndices(const fastgltf::Primitive &primitive, const fastgltf::Asset &asset, const uint32_t& offset) const
    {
        if (!primitive.indicesAccessor) {
            throw std::runtime_error("Indices accessor not found");
        }
        std::vector<uint32_t> indices;
        const auto &accessor = asset.accessors[primitive.indicesAccessor.value()];
        indices.reserve(accessor.count);
        fastgltf::iterateAccessor<uint32_t>(asset, accessor, [&](uint32_t index) { indices.push_back(index + offset); });
        return indices;
    }

    std::vector<VertexInput> GltfParser::parseVertexInputs(const fastgltf::Primitive &primitive, const fastgltf::Asset &asset) const
    {
        const auto &posAccessorIndex = primitive.findAttribute("POSITION")->accessorIndex;
        if (posAccessorIndex >= asset.accessors.size()) {
            throw std::runtime_error("Position accessor not found");
        }
        const auto &posAccessor = asset.accessors[posAccessorIndex];
        std::vector<VertexInput> vertex_inputs(posAccessor.count);

        // 使用辅助函数解析属性
        parseAttributeInternal<glm::vec3>(asset, posAccessorIndex, [&](glm::vec3 value, size_t index) { vertex_inputs[index].position = value; });
        
        auto colorAttr = primitive.findAttribute("COLOR_0");
        if (colorAttr) parseAttributeInternal<glm::vec4>(asset, colorAttr->accessorIndex, [&](glm::vec4 value, size_t index) { vertex_inputs[index].color = value; });
        
        auto normalAttr = primitive.findAttribute("NORMAL");
        if (normalAttr) parseAttributeInternal<glm::vec3>(asset, normalAttr->accessorIndex, [&](glm::vec3 value, size_t index) { vertex_inputs[index].normal = value; });
        
        auto tangentAttr = primitive.findAttribute("TANGENT");
        if (tangentAttr) parseAttributeInternal<glm::vec4>(asset, tangentAttr->accessorIndex, [&](glm::vec4 value, size_t index) { vertex_inputs[index].tangent = value; });
        
        auto uv0Attr = primitive.findAttribute("TEXCOORD_0");
        if (uv0Attr) parseAttributeInternal<glm::vec2>(asset, uv0Attr->accessorIndex, [&](glm::vec2 value, size_t index) { vertex_inputs[index].uv0 = value; });
        
        auto uv1Attr = primitive.findAttribute("TEXCOORD_1");
        if (uv1Attr) parseAttributeInternal<glm::vec2>(asset, uv1Attr->accessorIndex, [&](glm::vec2 value, size_t index) { vertex_inputs[index].uv1 = value; });

        return vertex_inputs;
    }

    uint32_t GltfParser::parseMaterialIndex(const fastgltf::Primitive &primitive) const
    {
        if (!primitive.materialIndex) {
            throw std::runtime_error("Material index not found");
        }
        return primitive.materialIndex.value();
    }

    template<typename T, typename Setter>
    void GltfParser::parseAttributeInternal(const fastgltf::Asset &asset, size_t accessorIndex, Setter setter) const
    {
        if (accessorIndex < asset.accessors.size()) {
            const auto &accessor = asset.accessors[accessorIndex];
            fastgltf::iterateAccessorWithIndex<T>(asset, accessor, setter, fastgltf::DefaultBufferDataAdapter{});
        }
    }

    std::vector<PerMeshData> GltfParser::BuildMeshList(const fastgltf::Asset &asset) const
    {
        std::vector<PerMeshData> meshes;
        meshes.reserve(asset.meshes.size());

        // collect all node transforms for each mesh
        std::vector<std::vector<glm::mat4>> meshTransforms(asset.meshes.size());
        for (const auto& node : asset.nodes) {
            if (node.meshIndex.has_value()) {
                size_t meshIndex = node.meshIndex.value();
                meshTransforms[meshIndex].push_back(parseTransform(node));
            }
        }

        // build mesh list
        for (size_t meshIndex = 0; meshIndex < asset.meshes.size(); ++meshIndex) {
            const auto& srcMesh = asset.meshes[meshIndex];
            const auto& transforms = meshTransforms[meshIndex];

            // skip meshes that are not referenced by any nodes
            if (transforms.empty()) {
                continue;
            }

            uint32_t start_index = 0;
            uint32_t start_vertex = 0;
            PerMeshData destMesh;
            destMesh.name = !srcMesh.name.empty() ? std::string(srcMesh.name) : "Mesh_" + std::to_string(meshIndex);
            destMesh.primitives.reserve(srcMesh.primitives.size() * transforms.size());
            for (const auto& primitive : srcMesh.primitives) 
            {
                // parse indices, vertex inputs and material index
                auto indices_result =               parseIndices(primitive, asset, start_vertex);
                auto vertexInputs_result = parseVertexInputs(primitive, asset);
                auto materialIndex =                             parseMaterialIndex(primitive);

                // count current vertex and index count
                uint32_t indexCount = indices_result.size();
                uint32_t vertexCount = vertexInputs_result.size();

                // build draw call data
                for (const auto& transform : transforms) {
                    destMesh.primitives.push_back({
                        .transform = transform,
                        .indices = std::move(indices_result),
                        .vertex_inputs = std::move(vertexInputs_result),
                        .material_index = materialIndex,
                        .first_index = start_index,
                        .index_count = indexCount,
                        .first_vertex = start_vertex,
                        .vertex_count = vertexCount
                    });
                }

                // update first index and first vertex
                start_index += indexCount;
                start_vertex += vertexCount;
            }
            meshes.push_back(std::move(destMesh));
        }
        return meshes;
    }

    std::vector<PerDrawCallData> GltfParser::BuildDrawCallDataList(const fastgltf::Asset &asset) const
    {
        std::vector<PerDrawCallData> drawCalls;
        uint32_t start_index = 0;
        uint32_t start_vertex = 0;
        // use ranges to process nodes and primitives, directly generate draw calls
        auto drawCallViews = asset.nodes
            | std::views::filter([](const auto &node) { return node.meshIndex.has_value(); })
            | std::views::transform([&](const auto &node) 
            {
                const auto &mesh = asset.meshes[node.meshIndex.value()];

                // parse transfrom
                auto transform = parseTransform(node);
                
                // zero out start_index and start_vertex
                start_index = 0;
                start_vertex = 0;
                
                return mesh.primitives
                    | std::views::transform([&](const auto &primitive) -> PerDrawCallData 
                    {
                        // parse indices, vertex inputs and material index
                        auto indices_result =               parseIndices(primitive, asset, start_vertex);
                        auto vertexInputs_result = parseVertexInputs(primitive, asset);
                        auto material_index =                            parseMaterialIndex(primitive);
                        
                        // count current vertex and index count
                        uint32_t indexCount = indices_result.size();
                        uint32_t vertexCount = vertexInputs_result.size();

                        // construct draw call data
                        PerDrawCallData drawCallData{
                            .transform = transform,
                            .indices = std::move(indices_result),
                            .vertex_inputs = std::move(vertexInputs_result),
                            .material_index = material_index,
                            .first_index = start_index,
                            .index_count = indexCount,
                            .first_vertex = start_vertex,
                            .vertex_count = vertexCount
                        };

                        // update first index and first vertex
                        start_index += indexCount;
                        start_vertex += vertexCount;

                        return drawCallData;
                    });
            })
            | std::views::join;

        drawCalls.reserve(std::ranges::distance(drawCallViews));
        std::ranges::copy(drawCallViews, std::back_inserter(drawCalls));

        return drawCalls;
    }
}

#endif // GLTF_PARSER_H