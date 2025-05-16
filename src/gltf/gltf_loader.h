#pragma once

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <ranges>
#include <iostream>

namespace gltf
{
    class GltfLoader
    {
    public:
        GltfLoader(const std::string_view& path);
        ~GltfLoader();

    private:
        fastgltf::Asset asset_;
    };
}

