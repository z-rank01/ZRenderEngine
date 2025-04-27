#pragma once

#include <vulkan/vulkan.h>
#include <unordered_set>

#include "builder/builder.h"
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
    VkInstance GetVulkanInstance() const { return vkInstance_; }
    

private:
    SVulkanInstanceConfig instance_config_;
    VkInstance vkInstance_;

    std::vector<const char*> ExtractValidationLayers();
    std::vector<const char*> ExtractExtensions();
};


class VulkanInstanceBuilder : public Builder<VkInstance>
{
private:
    typedef struct ApplicationInfo
    {
        const char* application_name;
        const char* engine_name;
        uint32_t application_version;
        uint32_t engine_version;
        uint32_t highest_api_version;
        void* pNext;
    } ApplicationInfo;

    typedef struct InstanceInfo
    {
        ApplicationInfo app_info;
        const char* const* required_layers;
        uint32_t required_layer_count;
        const char* const* required_extensions;
        uint32_t required_extension_count;
    } InstanceInfo;

    InstanceInfo instance_info_;
public:
    VulkanInstanceBuilder();
    ~VulkanInstanceBuilder();

    VkInstance Build() override;

    // Application Info
    VulkanInstanceBuilder& SetApplicationName(std::string name)
    {
        instance_info_.app_info.application_name = name.c_str();
        return *this;
    }
    VulkanInstanceBuilder& SetApplicationVersion(uint8_t major, uint8_t minor, uint8_t patch)
    {
        instance_info_.app_info.application_version = VK_MAKE_VERSION(major, minor, patch);
        return *this;
    }
    VulkanInstanceBuilder& SetEngineName(std::string name)
    {
        instance_info_.app_info.engine_name = name.c_str();
        return *this;
    }
    VulkanInstanceBuilder& SetEngineVersion(uint8_t major, uint8_t minor, uint8_t patch)
    {
        instance_info_.app_info.engine_version = VK_MAKE_VERSION(major, minor, patch);
        return *this;
    }
    VulkanInstanceBuilder& SetApiHighestVersion(uint8_t major, uint8_t minor, uint8_t patch)
    {
        instance_info_.app_info.highest_api_version = VK_MAKE_API_VERSION(0, major, minor, patch);
        return *this;
    }
    VulkanInstanceBuilder& SetApplicationNext(void* pNext)
    {
        instance_info_.app_info.pNext = pNext;
        return *this;
    }

    // layer
    VulkanInstanceBuilder& SetRequiredLayers(const char* const* layers, uint32_t count);

    // extension
    VulkanInstanceBuilder& SetRequiredExtensions(const char* const* extensions, uint32_t count);
};