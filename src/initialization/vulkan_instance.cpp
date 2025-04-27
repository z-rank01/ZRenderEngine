#pragma once

#include "vulkan_instance.h"


VulkanInstanceHelper::VulkanInstanceHelper(SVulkanInstanceConfig config) : instance_config_(config)
{
    vkInstance_ = VK_NULL_HANDLE;

    // Check if the instance configuration is valid
    if (instance_config_.application_name.empty() || instance_config_.engine_name.empty())
    {
        Logger::LogError("Application name or engine name is not set");
        return;
    }
}

VulkanInstanceHelper::~VulkanInstanceHelper()
{
    vkDestroyInstance(vkInstance_, nullptr);
}

bool VulkanInstanceHelper::CreateVulkanInstance()
{
    // Application info
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = instance_config_.application_name.c_str();
    app_info.applicationVersion = VK_MAKE_VERSION(instance_config_.application_version[0], instance_config_.application_version[1], instance_config_.application_version[2]);
    app_info.pEngineName = instance_config_.engine_name.c_str();
    app_info.engineVersion = VK_MAKE_VERSION(instance_config_.engine_version[0], instance_config_.engine_version[1], instance_config_.engine_version[2]);
    app_info.apiVersion = VK_MAKE_API_VERSION(instance_config_.api_version[0], instance_config_.api_version[1], instance_config_.api_version[2], instance_config_.api_version[3]);

    // extract validation layers
    instance_config_.validation_layers = ExtractValidationLayers();
    // extract extensions
    instance_config_.extensions = ExtractExtensions();

    // Instance info
    VkInstanceCreateInfo instance_info = {};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app_info;
    instance_info.enabledLayerCount = static_cast<uint32_t>(instance_config_.validation_layers.size());
    instance_info.ppEnabledLayerNames = instance_config_.validation_layers.data();
    instance_info.enabledExtensionCount = static_cast<uint32_t>(instance_config_.extensions.size());
    instance_info.ppEnabledExtensionNames = instance_config_.extensions.data();

    // Create Vulkan instance
    bool result = Logger::LogWithVkResult(
            vkCreateInstance(&instance_info, nullptr, &vkInstance_), 
            "Failed to create Vulkan instance", 
            "Succeeded in creating Vulkan instance");
    return result;
}

std::vector<const char*> VulkanInstanceHelper::ExtractValidationLayers()
{
    // retrieve the number of supported layers
    uint32_t validationCount = 0;
    vkEnumerateInstanceLayerProperties(&validationCount, nullptr);
    std::vector<VkLayerProperties> supported_layers(validationCount);
    vkEnumerateInstanceLayerProperties(&validationCount, supported_layers.data());

    // convert the supported layers to a set for faster lookup
    std::unordered_set<std::string> supported_layer_names;
    for (const auto& layer : supported_layers) 
    {
        supported_layer_names.insert(layer.layerName);
        Logger::LogDebug("Supported layer: " + std::string(layer.layerName));
    }

    // filter out the required layers that are not supported
    std::vector<const char*> available_layers;
    available_layers.reserve(instance_config_.validation_layers.size());
    for (const auto& layer : instance_config_.validation_layers) 
    {
        if (supported_layer_names.find(layer) != supported_layer_names.end()) 
        {
            available_layers.push_back(layer);
        }
        else 
        {
            Logger::LogWarning("Required layer not supported: " + std::string(layer));
        }
    }

    return available_layers;
}

std::vector<const char*> VulkanInstanceHelper::ExtractExtensions()
{
    // retrieve the number of supported extensions
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> supported_extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, supported_extensions.data());

    // convert the supported extensions to a set for faster lookup
    std::unordered_set<std::string> supported_extension_names;
    for (const auto& extension : supported_extensions) 
    {
        supported_extension_names.insert(extension.extensionName);
    }

    // filter out the required extensions that are not supported
    std::vector<const char*> available_extensions;
    available_extensions.reserve(instance_config_.extensions.size());
    for (const auto& ext : instance_config_.extensions) 
    {
        if (supported_extension_names.find(ext) != supported_extension_names.end()) 
        {
            available_extensions.push_back(ext);
        }
        else 
        {
            Logger::LogWarning("Required extension not supported: " + std::string(ext));
        }
    }

    return available_extensions;
}

// ------------------------------------
// VulkanInstanceBuilder Implementation
// ------------------------------------

VulkanInstanceBuilder::VulkanInstanceBuilder()
{
}

VulkanInstanceBuilder::~VulkanInstanceBuilder()
{
}

VkInstance VulkanInstanceBuilder::Build()
{
    // Create Vulkan instance
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = instance_info_.app_info.application_name;
    app_info.applicationVersion = instance_info_.app_info.application_version;
    app_info.pEngineName = instance_info_.app_info.engine_name;
    app_info.engineVersion = instance_info_.app_info.engine_version;
    app_info.apiVersion = instance_info_.app_info.highest_api_version;
    app_info.pNext = instance_info_.app_info.pNext;

    VkInstanceCreateInfo instance_info = {};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app_info;
    instance_info.enabledLayerCount = instance_info_.required_layer_count;
    instance_info.ppEnabledLayerNames = instance_info_.required_layers;
    instance_info.enabledExtensionCount = instance_info_.required_extension_count;
    instance_info.ppEnabledExtensionNames = instance_info_.required_extensions;
    instance_info.pNext = nullptr;

    VkInstance instance;
    VkResult result = vkCreateInstance(&instance_info, nullptr, &instance);
    if (result != VK_SUCCESS)
    {
        std::cerr << "Failed to create Vulkan instance: " << result << std::endl;
        return VK_NULL_HANDLE;
    }
    return instance;
}

VulkanInstanceBuilder& VulkanInstanceBuilder::SetRequiredLayers(const char* const* layers, uint32_t count)
{
    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    std::vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

    // create hash set of available layers
    std::unordered_set<std::string> available_layer_names;
    for (const auto& layer : available_layers)
    {
        available_layer_names.insert(layer.layerName);
    }

    // filter out the required layers that are not supported
    std::vector<const char*> filtered_layers;
    for (uint32_t i = 0; i < count; ++i)
    {
        if (available_layer_names.find(layers[i]) != available_layer_names.end())
        {
            filtered_layers.push_back(layers[i]);
        }
        else
        {
            std::cerr << "Layer not supported: " << layers[i] << std::endl;
        }
    }

    // set the filtered layers
    instance_info_.required_layers = filtered_layers.data();
    instance_info_.required_layer_count = static_cast<uint32_t>(filtered_layers.size());
    return *this;
}

VulkanInstanceBuilder& VulkanInstanceBuilder::SetRequiredExtensions(const char* const* extensions, uint32_t count)
{
    uint32_t extension_count;
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, available_extensions.data());

    // create hash set of available extensions
    std::unordered_set<std::string> available_extension_names;
    for (const auto& extension : available_extensions)
    {
        available_extension_names.insert(extension.extensionName);
    }

    // filter out the required extensions that are not supported
    std::vector<const char*> filtered_extensions;
    for (uint32_t i = 0; i < count; ++i)
    {
        if (available_extension_names.find(extensions[i]) != available_extension_names.end())
        {
            filtered_extensions.push_back(extensions[i]);
        }
        else
        {
            std::cerr << "Extension not supported: " << extensions[i] << std::endl;
        }
    }

    // set the filtered extensions
    instance_info_.required_extensions = filtered_extensions.data();
    instance_info_.required_extension_count = static_cast<uint32_t>(filtered_extensions.size());
    return *this;
}
