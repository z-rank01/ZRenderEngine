#include "test.h"
#include <fstream>
#include <nlohmann/json.hpp>

void TestVulkanglTFModel::LoadGLTF(std::string filename)
{
    tinygltf::Model glTFInput;
    tinygltf::TinyGLTF gltfContext;
    std::string error, warning;

#if defined(__ANDROID__)
    // On Android all assets are packed with the apk in a compressed form, so we need to open them using the asset manager
    // We let tinygltf handle this, by passing the asset manager of our app
    tinygltf::asset_manager = androidApp->activity->assetManager;
#endif
    bool fileLoaded = gltfContext.LoadASCIIFromFile(&glTFInput, &error, &warning, filename);

    // Pass some Vulkan resources required for setup and rendering to the glTF model loading class
    glTFModel.vulkanDevice = vulkanDevice;
    glTFModel.copyQueue = vulkanQueue;

    std::vector<uint32_t> indexBuffer;
    std::vector<VulkanglTFModel::Vertex> vertexBuffer;

    if (fileLoaded) {
        glTFModel.loadImages(glTFInput);
        glTFModel.loadMaterials(glTFInput);
        glTFModel.loadTextures(glTFInput);
        const tinygltf::Scene& scene = glTFInput.scenes[0];
        for (size_t i = 0; i < scene.nodes.size(); i++) {
            const tinygltf::Node node = glTFInput.nodes[scene.nodes[i]];
            glTFModel.loadNode(node, glTFInput, nullptr, indexBuffer, vertexBuffer);
        }
    }
    else {
        std::cerr << ("Could not open the glTF file.\n\nMake sure the assets submodule has been checked out and is up-to-date.", -1) << std::endl;
        return;
    }

    // Create and upload vertex and index buffer
    // We will be using one single vertex buffer and one single index buffer for the whole glTF scene
    // Primitives (of the glTF model) will then index into these using index offsets

    size_t vertexBufferSize = vertexBuffer.size() * sizeof(VulkanglTFModel::Vertex);
    size_t indexBufferSize = indexBuffer.size() * sizeof(uint32_t);
    glTFModel.indices.count = static_cast<uint32_t>(indexBuffer.size());

    struct StagingBuffer {
        VkBuffer buffer;
        VkDeviceMemory memory;
    } vertexStaging, indexStaging;

    // // Create host visible staging buffers (source)
    // VK_CHECK_RESULT(vulkanDevice->createBuffer(
    // 	VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    // 	VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    // 	vertexBufferSize,
    // 	&vertexStaging.buffer,
    // 	&vertexStaging.memory,
    // 	vertexBuffer.data()));
    // // Index data
    // VK_CHECK_RESULT(vulkanDevice->createBuffer(
    // 	VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    // 	VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    // 	indexBufferSize,
    // 	&indexStaging.buffer,
    // 	&indexStaging.memory,
    // 	indexBuffer.data()));

    // // Create device local buffers (target)
    // VK_CHECK_RESULT(vulkanDevice->createBuffer(
    // 	VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    // 	VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    // 	vertexBufferSize,
    // 	&glTFModel.vertices.buffer,
    // 	&glTFModel.vertices.memory));
    // VK_CHECK_RESULT(vulkanDevice->createBuffer(
    // 	VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    // 	VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    // 	indexBufferSize,
    // 	&glTFModel.indices.buffer,
    // 	&glTFModel.indices.memory));

    // // Free staging resources
    // vkDestroyBuffer(vulkanDevice, vertexStaging.buffer, nullptr);
    // vkFreeMemory(vulkanDevice, vertexStaging.memory, nullptr);
    // vkDestroyBuffer(vulkanDevice, indexStaging.buffer, nullptr);
    // vkFreeMemory(vulkanDevice, indexStaging.memory, nullptr);

    // 导出顶点和索引数据到JSON文件
    // ExportBuffersToJson(vertexBuffer, indexBuffer, filename);
}

void TestVulkanglTFModel::ExportBuffersToJson(const std::vector<VulkanglTFModel::Vertex>& vertexBuffer,
                                            const std::vector<uint32_t>& indexBuffer,
                                            const std::string& sourceFilename)
{
    using json = nlohmann::json;

    // 从源文件名中提取文件名部分（不含路径和扩展名）
    std::string baseName = sourceFilename;
    size_t lastSlash = baseName.find_last_of("/\\");
    if (lastSlash != std::string::npos)
        baseName = baseName.substr(lastSlash + 1);
    size_t lastDot = baseName.find_last_of(".");
    if (lastDot != std::string::npos)
        baseName = baseName.substr(0, lastDot);

    // 构建输出文件路径
    std::string vertexOutputPath = "E:\\Projects\\ZRenderGraph\\output\\" + baseName + "_vertices.json";
    std::string indexOutputPath = "E:\\Projects\\ZRenderGraph\\output\\" + baseName + "_indices.json";

    // 确保输出目录存在
    std::string outputDir = "E:\\Projects\\ZRenderGraph\\output";
    std::filesystem::create_directories(outputDir);

    // 导出顶点数据
    json vertexJson = json::array();
    for (const auto& vertex : vertexBuffer) {
        json vertexObj;
        vertexObj["position"] = {vertex.pos.x, vertex.pos.y, vertex.pos.z};
        vertexObj["normal"] = {vertex.normal.x, vertex.normal.y, vertex.normal.z};
        vertexObj["uv"] = {vertex.uv.x, vertex.uv.y};
        vertexObj["color"] = {vertex.color.r, vertex.color.g, vertex.color.b};
        vertexJson.push_back(vertexObj);
    }

    // 导出索引数据
    json indexJson = json::array();
    for (const auto& index : indexBuffer) {
        indexJson.push_back(index);
    }

    // 写入文件
    try {
        std::ofstream vertexFile(vertexOutputPath);
        vertexFile << std::setw(4) << vertexJson << std::endl;
        vertexFile.close();

        std::ofstream indexFile(indexOutputPath);
        indexFile << std::setw(4) << indexJson << std::endl;
        indexFile.close();

        std::cout << "Exported vertex data to: " << vertexOutputPath << std::endl;
        std::cout << "Exported index data to: " << indexOutputPath << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to export buffers to JSON: " << e.what() << std::endl;
    }
}
