#pragma once

#include <vulkan/vulkan.h>
#include <vector>

typedef enum EPhysicalDeviceFeatures
{
    RobustBufferAccess = 0,
    FullDrawIndexUint32,
    ImageCubeArray,
    IndependentBlend,
    GeometryShader,
    TessellationShader,
    SampleRateShading,
    DualSrcBlend,
    LogicOp,
    MultiDrawIndirect,
    DrawIndirectFirstInstance,
    DepthClamp,
    DepthBiasClamp,
    FillModeNonSolid,
    DepthBounds,
    WideLines,
    LargePoints,
    AlphaToOne,
    MultiViewport,
    SamplerAnisotropy,
    TextureCompressionETC2,
    TextureCompressionASTC_LDR,
    TextureCompressionBC,
    OcclusionQueryPrecise,
    PipelineStatisticsQuery,
    VertexPipelineStoresAndAtomics,
    FragmentStoresAndAtomics,
    ShaderTessellationAndGeometryPointSize,
    ShaderImageGatherExtended,
    ShaderStorageImageExtendedFormats,
    ShaderStorageImageMultisample,
    ShaderStorageImageReadWithoutFormat,
    ShaderStorageImageWriteWithoutFormat,
    ShaderUniformBufferArrayDynamicIndexing,
    ShaderSampledImageArrayDynamicIndexing,
    ShaderStorageBufferArrayDynamicIndexing,
    ShaderStorageImageArrayDynamicIndexing,
    ShaderClipDistance,
    ShaderCullDistance,
    ShaderFloat64,
    ShaderInt64,
    ShaderInt16, 
    ShaderResourceResidency,
    ShaderResourceMinLod,
    SparseBinding,
    SparseResidencyBuffer,
    SparseResidencyImage2D,
    SparseResidencyImage3D,
    SparseResidency2Samples,
    SparseResidency4Samples,
    SparseResidency8Samples,
    SparseResidency16Samples,
    SparseResidencyAliased,
    VariableMultisampleRate,
    InheritedQueries,
} EPhysicalDeviceFeatures;

struct SVulkanDeviceConfig
{
    // physical device config
    // properties
    VkPhysicalDeviceType physical_device_type;
    uint8_t physical_device_api_version[4];
    // features
    std::vector<EPhysicalDeviceFeatures> physical_device_features;
    
    // Queue and Queue Family config
    std::vector<VkQueueFlagBits> queue_flags;
};

class VulkanDeviceHelper
{
public:
    VulkanDeviceHelper();
    VulkanDeviceHelper(SVulkanDeviceConfig config);
    ~VulkanDeviceHelper();

    bool CreateLogicalDevice(const VkDeviceQueueCreateInfo& queue_create_info);
    bool CreatePhysicalDevice(const VkInstance& instance);

    const VkPhysicalDevice& GetPhysicalDevice() const { return vkPhysicalDevice_; }
    const VkDevice& GetLogicalDevice() const { return vkLogicalDevice_; }

private:
    SVulkanDeviceConfig device_config_;
    VkPhysicalDevice vkPhysicalDevice_;
    VkDevice vkLogicalDevice_;

    VkPhysicalDevice PickPhysicalDevice(const std::vector<VkPhysicalDevice>& instance);
};