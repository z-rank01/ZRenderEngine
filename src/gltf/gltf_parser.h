#ifndef GLTF_PARSER_H
#define GLTF_PARSER_H

#include <tiny_gltf.h>
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
        std::vector<PerMeshData> operator()(const tinygltf::Model &asset, RequestMeshList) const;

        /// @brief Parse gltf file and return draw call data list
        /// @param asset: gltf asset
        /// @param request: request type
        /// @return: draw call data list
        std::vector<PerDrawCallData> operator()(const tinygltf::Model &asset, RequestDrawCallList) const;

    private:
        glm::mat4                       parseTransform(const tinygltf::Node &node) const;
        std::vector<uint16_t>           parseIndices(const tinygltf::Primitive &primitive, const tinygltf::Model &asset, const uint16_t &offset) const;
        std::vector<VertexInput>        parseVertexInputs(const tinygltf::Primitive &primitive, const tinygltf::Model &asset) const;
        uint16_t                        parseMaterialIndex(const tinygltf::Primitive &primitive) const;

        std::vector<PerMeshData>        BuildMeshList(const tinygltf::Model &asset) const;
        std::vector<PerDrawCallData>    BuildDrawCallDataList(const tinygltf::Model &asset) const;

        template<typename T, typename Setter>
        void parseAttributeInternal(const tinygltf::Model &asset, size_t accessorIndex, Setter setter) const;
    };
}

#endif // GLTF_PARSER_H