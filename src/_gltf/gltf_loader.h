#ifndef GLTF_LOADER_H
#define GLTF_LOADER_H

#include <tiny_gltf.h>
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
        /// @param path: path to the gltf filp
        /// @return: gltf asset
        tinygltf::Model operator()(const std::string_view& path);
    };

    inline tinygltf::Model GltfLoader::operator()(const std::string_view &path)
    {
        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        std::string err;
        std::string warn;

        bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, std::string(path));

        if (!warn.empty()) {
            std::cerr << "GLTF Warning: " << warn << std::endl;
        }

        if (!err.empty()) {
            std::cerr << "GLTF Error: " << err << std::endl;
        }

        if (!ret) {
            throw std::runtime_error("Failed to load gltf file: " + err);
        }

        return model;
    }
}

#endif // GLTF_LOADER_H
