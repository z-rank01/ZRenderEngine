#ifndef GLTF_LOADER_H
#define GLTF_LOADER_H

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <ranges>
#include <iostream>

namespace gltf
{
    class GltfLoader
    {
    public:
        GltfLoader() = default;
        ~GltfLoader() = default;

        /// @brief Load gltf file from path
        /// @param path: path to the gltf file
        /// @return: gltf asset
        fastgltf::Asset operator()(const std::string_view& path);
    };

    inline fastgltf::Asset GltfLoader::operator()(const std::string_view &path)
    {
        constexpr auto gltfOptions =
            fastgltf::Options::DontRequireValidAssetMember |
            fastgltf::Options::AllowDouble |
            fastgltf::Options::LoadExternalBuffers |
            fastgltf::Options::GenerateMeshIndices;

        // load the gltf file
        auto buffer = fastgltf::GltfDataBuffer::FromPath(std::string(path));
        if (buffer.error() != fastgltf::Error::None)
        {
            std::cerr << fastgltf::getErrorMessage(buffer.error()) << std::endl;
            throw std::runtime_error("Failed to load gltf file");
        }

        // parse the gltf file
        fastgltf::Parser parser;
        auto result = parser.loadGltf(
            buffer.get(),
            std::filesystem::path(path).parent_path(),
            gltfOptions);

        if (result.error() != fastgltf::Error::None)
        {
            std::cerr << fastgltf::getErrorMessage(result.error()) << std::endl;
            throw std::runtime_error("Failed to parse gltf file");
        }

        return std::move(result.get());
    }
}

#endif // GLTF_LOADER_H
