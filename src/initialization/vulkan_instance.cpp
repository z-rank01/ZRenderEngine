#pragma once

#include "vulkan_instance.h"
#include <unordered_set>

VulkanInstanceHelper::VulkanInstanceHelper(SVulkanInstanceConfig config)
{
    instance_config_ = config;
}

VulkanInstanceHelper::~VulkanInstanceHelper()
{
}

bool VulkanInstanceHelper::CreateVulkanInstance(VkInstance& instance)
{
    // Application info
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = instance_config_.application_name.c_str();
    app_info.applicationVersion = VK_MAKE_VERSION(instance_config_.application_version[0], instance_config_.application_version[1], instance_config_.application_version[2]);
    app_info.pEngineName = instance_config_.engine_name.c_str();
    app_info.engineVersion = VK_MAKE_VERSION(instance_config_.engine_version[0], instance_config_.engine_version[1], instance_config_.engine_version[2]);
    app_info.apiVersion = VK_API_VERSION_1_2;

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
            vkCreateInstance(&instance_info, nullptr, &instance), 
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