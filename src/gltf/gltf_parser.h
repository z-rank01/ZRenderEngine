#ifndef GLTF_PARSER_H
#define GLTF_PARSER_H

#include <fastgltf/core.hpp>
#include <algorithm>
#include <ranges>
#include <vector>
#include "gltf_data.h"

namespace gltf
{
    class GltfParser
    {
    public:
        GltfParser(const fastgltf::Asset &asset);
    };

    GltfParser::GltfParser(const fastgltf::Asset &asset)
    {
        std::vector<Mesh> meshes;
        auto mesh_range = asset.nodes
            | std::views::filter([](const fastgltf::Node &node) {
                return node.meshIndex.has_value();
              })
            | std::views::transform([&asset](const fastgltf::Node &node) -> Mesh {
                Mesh filtered_mesh;
                // TODO: filter mesh
                return filtered_mesh;
              });

        // copy the result to the meshes vector
        // std::ranges::copy(mesh_range, std::back_inserter(meshes));
    }
}

#endif // GLTF_PARSER_H