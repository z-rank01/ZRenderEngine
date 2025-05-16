#include "gltf_loader.h"

gltf::GltfLoader::GltfLoader(const std::string_view& path)
{
    constexpr auto gltfOptions = 
        fastgltf::Options::DontRequireValidAssetMember | 
        fastgltf::Options::AllowDouble | 
        fastgltf::Options::LoadExternalBuffers;

    auto buffer = fastgltf::GltfDataBuffer::FromPath(std::string(path));
    if (buffer.error() != fastgltf::Error::None)
    {
        std::cerr << fastgltf::getErrorMessage(buffer.error()) << std::endl;
        throw std::runtime_error("Failed to load gltf file");
    }
    
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
    
    asset_ = std::move(result.get()); 
}

gltf::GltfLoader::~GltfLoader()
{
}
