#pragma once

#include <vulkan/vulkan.h>
#include "utility/logger.h"

struct SVulkanInstanceConfig
{
    // Application info
    std::string application_name;
    uint8_t application_version[3];

    // Engine info
    std::string engine_name;
    uint8_t engine_version[3];

    // Vulkan API version
    uint8_t api_version[4];
    
    // validation info
    std::vector<const char*> validation_layers;

    // extension info
    std::vector<const char*> extensions;
};

class VulkanInstanceHelper
{
public:
    VulkanInstanceHelper() = delete;
    VulkanInstanceHelper(SVulkanInstanceConfig config);
    ~VulkanInstanceHelper();

    bool CreateVulkanInstance();
    const VkInstance& GetVulkanInstance() const { return vkInstance_; }
    

private:
    SVulkanInstanceConfig instance_config_;
    VkInstance vkInstance_;

    std::vector<const char*> ExtractValidationLayers();
    std::vector<const char*> ExtractExtensions();
};