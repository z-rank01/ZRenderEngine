#ifndef COMMON_H
#define COMMON_H

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <optional>
#include <ranges>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

#include "_callable/callable.h"

namespace templates::common
{

/// -------------------------------------------
/// vulkan instance templated functions: common
/// -------------------------------------------

/// @brief Vulkan instance context with lazy evaluation support
struct CommVkInstanceContext
{
    // application info
    struct ApplicationInfo
    {
        std::string application_name_ = "Vulkan Engine";
        std::string engine_name_      = "Vulkan Engine";
        uint32_t application_version_ = VK_MAKE_VERSION(1, 0, 0);
        uint32_t engine_version_      = VK_MAKE_VERSION(1, 0, 0);
        uint32_t highest_api_version_ = VK_API_VERSION_1_3;
        void* p_next_                 = nullptr;
    } app_info_;

    // instance info
    struct InstanceInfo
    {
        ApplicationInfo app_info_;
        std::vector<const char*> required_layers_;
        std::vector<const char*> required_extensions_;
    } instance_info_;

    // vulkan natives
    VkInstance vk_instance_ = VK_NULL_HANDLE;
};

/// @brief Instance functions using Monad-Like Chain
namespace instance
{

/// @brief Creates initial context
inline auto create_context()
{
    return callable::make_chain(CommVkInstanceContext{});
}

/// @brief Sets application name
inline auto set_application_name(const std::string& name)
{
    return [name](CommVkInstanceContext ctx) -> callable::Chainable<CommVkInstanceContext>
    {
        ctx.app_info_.application_name_ = name;
        return callable::make_chain(std::move(ctx));
    };
}

/// @brief Sets engine name
inline auto set_engine_name(const std::string& name)
{
    return [name](CommVkInstanceContext ctx) -> callable::Chainable<CommVkInstanceContext>
    {
        ctx.app_info_.engine_name_ = name;
        return callable::make_chain(std::move(ctx));
    };
}

/// @brief Sets application version
inline auto set_application_version(uint32_t major, uint32_t minor, uint32_t patch)
{
    return [major, minor, patch](CommVkInstanceContext ctx) -> callable::Chainable<CommVkInstanceContext>
    {
        ctx.app_info_.application_version_ = VK_MAKE_VERSION(major, minor, patch);
        return callable::make_chain(std::move(ctx));
    };
}

/// @brief Sets engine version
inline auto set_engine_version(uint32_t major, uint32_t minor, uint32_t patch)
{
    return [major, minor, patch](CommVkInstanceContext ctx) -> callable::Chainable<CommVkInstanceContext>
    {
        ctx.app_info_.engine_version_ = VK_MAKE_VERSION(major, minor, patch);
        return callable::make_chain(std::move(ctx));
    };
}

/// @brief Sets API version
inline auto set_api_version(uint32_t api_version)
{
    return [api_version](CommVkInstanceContext ctx) -> callable::Chainable<CommVkInstanceContext>
    {
        ctx.app_info_.highest_api_version_ = api_version;
        return callable::make_chain(std::move(ctx));
    };
}

/// @brief Adds validation layers
inline auto add_validation_layers(const std::vector<const char*>& layers)
{
    return [layers](CommVkInstanceContext ctx) -> callable::Chainable<CommVkInstanceContext>
    {
        ctx.instance_info_.required_layers_.insert(
            ctx.instance_info_.required_layers_.end(), layers.begin(), layers.end());
        return callable::make_chain(std::move(ctx));
    };
}

/// @brief Adds extensions
inline auto add_extensions(const std::vector<const char*>& extensions)
{
    return [extensions](CommVkInstanceContext ctx) -> callable::Chainable<CommVkInstanceContext>
    {
        ctx.instance_info_.required_extensions_.insert(
            ctx.instance_info_.required_extensions_.end(), extensions.begin(), extensions.end());
        return callable::make_chain(std::move(ctx));
    };
}

/// @brief Creates the Vulkan instance (final step)
inline auto create_vk_instance()
{
    return [](CommVkInstanceContext ctx) -> callable::Chainable<CommVkInstanceContext>
    {
        VkApplicationInfo app_info{};
        app_info.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pNext              = ctx.app_info_.p_next_;
        app_info.pApplicationName   = ctx.app_info_.application_name_.c_str();
        app_info.applicationVersion = ctx.app_info_.application_version_;
        app_info.pEngineName        = ctx.app_info_.engine_name_.c_str();
        app_info.engineVersion      = ctx.app_info_.engine_version_;
        app_info.apiVersion         = ctx.app_info_.highest_api_version_;

        VkInstanceCreateInfo create_info{};
        create_info.sType             = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo  = &app_info;
        create_info.enabledLayerCount = static_cast<uint32_t>(ctx.instance_info_.required_layers_.size());
        create_info.ppEnabledLayerNames =
            ctx.instance_info_.required_layers_.empty() ? nullptr : ctx.instance_info_.required_layers_.data();
        create_info.enabledExtensionCount = static_cast<uint32_t>(ctx.instance_info_.required_extensions_.size());
        create_info.ppEnabledExtensionNames =
            ctx.instance_info_.required_extensions_.empty() ? nullptr : ctx.instance_info_.required_extensions_.data();

        VkResult result = vkCreateInstance(&create_info, nullptr, &ctx.vk_instance_);
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Vulkan instance. Error code: " + std::to_string(result));
        }

        return callable::make_chain(std::move(ctx));
    };
}

/// @brief Validates context before creation
inline auto validate_context()
{
    return [](CommVkInstanceContext ctx) -> callable::Chainable<CommVkInstanceContext>
    {
        if (ctx.app_info_.application_name_.empty())
        {
            return callable::Chainable<CommVkInstanceContext>(
                callable::error<CommVkInstanceContext>("Application name cannot be empty"));
        }
        if (ctx.app_info_.engine_name_.empty())
        {
            return callable::Chainable<CommVkInstanceContext>(
                callable::error<CommVkInstanceContext>("Engine name cannot be empty"));
        }
        return callable::make_chain(std::move(ctx));
    };
}

} // namespace instance

/// --------------------------------------------------
/// vulkan physical device templated functions: common
/// --------------------------------------------------

/// @brief Vulkan physical device context with lazy evaluation support
struct CommVkPhysicalDeviceContext
{
    // parent instance context
    VkInstance vk_instance_ = VK_NULL_HANDLE;

    // physical device selection criteria
    struct SelectionCriteria
    {
        std::optional<VkPhysicalDeviceType> preferred_device_type_ = std::nullopt;
        std::optional<uint32_t> minimum_api_version_               = std::nullopt;
        VkSurfaceKHR surface_                                      = VK_NULL_HANDLE;

        // required features
        VkPhysicalDeviceFeatures required_features_{};
        VkPhysicalDeviceVulkan11Features required_features_11_{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
        VkPhysicalDeviceVulkan12Features required_features_12_{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        VkPhysicalDeviceVulkan13Features required_features_13_{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};

        // required extensions
        std::vector<const char*> required_extensions_;

        // queue requirements
        struct QueueRequirement
        {
            VkQueueFlags queue_flags_;
            uint32_t min_queue_count_     = 1;
            bool require_present_support_ = false;
        };
        std::vector<QueueRequirement> queue_requirements_;

        // memory requirements
        std::optional<VkDeviceSize> minimum_device_memory_ = std::nullopt;
        std::optional<VkDeviceSize> minimum_host_memory_   = std::nullopt;

        // scoring preferences
        bool prefer_discrete_gpu_             = true;
        bool prefer_dedicated_graphics_queue_ = true;
    } selection_criteria_;

    // vulkan natives
    VkPhysicalDevice vk_physical_device_ = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties device_properties_{};
    VkPhysicalDeviceFeatures device_features_{};
    VkPhysicalDeviceMemoryProperties memory_properties_{};
    std::vector<VkQueueFamilyProperties> queue_family_properties_;
    std::vector<VkExtensionProperties> available_extensions_;
};

namespace physicaldevice
{

/// @brief Creates initial physical device context from instance context
/// @param instance Vulkan instance handle
/// @return CommVkPhysicalDeviceContext with instance set
inline auto create_physical_device_context(VkInstance instance)
{
    return callable::make_chain(
        [instance]() -> CommVkPhysicalDeviceContext
        {
            CommVkPhysicalDeviceContext ctx;
            ctx.vk_instance_ = instance;
            return ctx;
        }());
}

/// @brief Sets surface for present support checking
/// @param surface Vulkan surface handle
/// @return Callable that has set the surface in the context
inline auto set_surface(VkSurfaceKHR surface)
{
    return [surface](CommVkPhysicalDeviceContext ctx) -> callable::Chainable<CommVkPhysicalDeviceContext>
    {
        ctx.selection_criteria_.surface_ = surface;
        return callable::make_chain(std::move(ctx));
    };
}

/// @brief Sets minimum API version requirement
/// @param major Major version number
/// @param minor Minor version number
/// @param patch Patch version number (default is 0)
/// @return Callable that has set the minimum API version in the context
inline auto require_api_version(uint32_t major, uint32_t minor, uint32_t patch = 0)
{
    return [major, minor, patch](CommVkPhysicalDeviceContext ctx) -> callable::Chainable<CommVkPhysicalDeviceContext>
    {
        ctx.selection_criteria_.minimum_api_version_ = VK_MAKE_VERSION(major, minor, patch);
        return callable::make_chain(std::move(ctx));
    };
}

/// @brief Adds required device extensions
/// @param extensions List of extension names to require
/// @return Callable that has added the required extensions to the context
inline auto require_extensions(const std::vector<const char*>& extensions)
{
    return [extensions](CommVkPhysicalDeviceContext ctx) -> callable::Chainable<CommVkPhysicalDeviceContext>
    {
        ctx.selection_criteria_.required_extensions_.insert(
            ctx.selection_criteria_.required_extensions_.end(), extensions.begin(), extensions.end());
        return callable::make_chain(std::move(ctx));
    };
}

/// @brief Sets required Vulkan 1.0 features
/// @param features Vulkan physical device features to require
/// @return Callable that has set the required features in the context
inline auto require_features(const VkPhysicalDeviceFeatures& features)
{
    return [features](CommVkPhysicalDeviceContext ctx) -> callable::Chainable<CommVkPhysicalDeviceContext>
    {
        ctx.selection_criteria_.required_features_ = features;
        return callable::make_chain(std::move(ctx));
    };
}

/// @brief Sets required Vulkan 1.1 features
/// @param features Vulkan 1.1 physical device features to require
/// @return Callable that has set the required features in the context
inline auto require_features_11(const VkPhysicalDeviceVulkan11Features& features)
{
    return [features](CommVkPhysicalDeviceContext ctx) -> callable::Chainable<CommVkPhysicalDeviceContext>
    {
        ctx.selection_criteria_.required_features_11_ = features;
        return callable::make_chain(std::move(ctx));
    };
}

/// @brief Sets required Vulkan 1.2 features
/// @param features Vulkan 1.2 physical device features to require
/// @return Callable that has set the required features in the context
inline auto require_features_12(const VkPhysicalDeviceVulkan12Features& features)
{
    return [features](CommVkPhysicalDeviceContext ctx) -> callable::Chainable<CommVkPhysicalDeviceContext>
    {
        ctx.selection_criteria_.required_features_12_ = features;
        return callable::make_chain(std::move(ctx));
    };
}

/// @brief Sets required Vulkan 1.3 features
/// @param features Vulkan 1.3 physical device features to require
/// @return Callable that has set the required features in the context
inline auto require_features_13(const VkPhysicalDeviceVulkan13Features& features)
{
    return [features](CommVkPhysicalDeviceContext ctx) -> callable::Chainable<CommVkPhysicalDeviceContext>
    {
        ctx.selection_criteria_.required_features_13_ = features;
        return callable::make_chain(std::move(ctx));
    };
}

/// @brief Adds queue family requirement
/// @param queue_flags Vulkan queue flags to require (e.g., VK_QUEUE_GRAPHICS_BIT)
/// @param min_count Minimum number of queues required (default is 1)
/// @param require_present Whether the queue must support present (default is true)
/// @return Callable that has added the queue requirement to the context
inline auto require_queue(VkQueueFlags queue_flags, uint32_t min_count = 1, bool require_present = true)
{
    return [queue_flags, min_count, require_present](
               CommVkPhysicalDeviceContext ctx) -> callable::Chainable<CommVkPhysicalDeviceContext>
    {
        CommVkPhysicalDeviceContext::SelectionCriteria::QueueRequirement req;
        req.queue_flags_             = queue_flags;
        req.min_queue_count_         = min_count;
        req.require_present_support_ = require_present;
        ctx.selection_criteria_.queue_requirements_.push_back(req);
        return callable::make_chain(std::move(ctx));
    };
}

/// @brief Sets minimum device memory requirement
/// @param min_memory Minimum device memory size in bytes
/// @return Callable that has set the minimum device memory in the context
inline auto require_minimum_device_memory(VkDeviceSize min_memory)
{
    return [min_memory](CommVkPhysicalDeviceContext ctx) -> callable::Chainable<CommVkPhysicalDeviceContext>
    {
        ctx.selection_criteria_.minimum_device_memory_ = min_memory;
        return callable::make_chain(std::move(ctx));
    };
}

/// @brief Enables discrete GPU preference
/// @param prefer Whether to prefer discrete GPUs (default is true)
/// @return Callable that has set the discrete GPU preference in the context
inline auto prefer_discrete_gpu(bool prefer = true)
{
    return [prefer](CommVkPhysicalDeviceContext ctx) -> callable::Chainable<CommVkPhysicalDeviceContext>
    {
        ctx.selection_criteria_.prefer_discrete_gpu_ = prefer;
        return callable::make_chain(std::move(ctx));
    };
}

/// @brief Validates device meets all requirements
/// @return Callable that checks if the selected physical device meets all requirements
inline auto validate_device_requirements()
{
    return [](CommVkPhysicalDeviceContext ctx) -> callable::Chainable<CommVkPhysicalDeviceContext>
    {
        if (ctx.vk_physical_device_ == VK_NULL_HANDLE)
        {
            return callable::Chainable<CommVkPhysicalDeviceContext>(
                callable::error<CommVkPhysicalDeviceContext>("No physical device selected"));
        }

        // Check API version
        if (ctx.selection_criteria_.minimum_api_version_.has_value())
        {
            if (ctx.device_properties_.apiVersion < ctx.selection_criteria_.minimum_api_version_.value())
            {
                return callable::Chainable<CommVkPhysicalDeviceContext>(
                    callable::error<CommVkPhysicalDeviceContext>("Device API version insufficient"));
            }
        }

        // Check required extensions
        for (const char* required_ext : ctx.selection_criteria_.required_extensions_)
        {
            bool found = false;
            for (const auto& available_ext : ctx.available_extensions_)
            {
                // 使用更安全的字符串比较方式
                if (std::string_view(required_ext) == std::string_view(available_ext.extensionName))
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                return callable::Chainable<CommVkPhysicalDeviceContext>(callable::error<CommVkPhysicalDeviceContext>(
                    std::string("Required extension not available: ") + required_ext));
            }
        }

        return callable::make_chain(std::move(ctx));
    };
}

/// @brief Selects the best physical device (final step)
inline auto select_physical_device()
{
    return [](CommVkPhysicalDeviceContext ctx) -> callable::Chainable<CommVkPhysicalDeviceContext>
    {
        // Enumerate physical devices
        uint32_t device_count = 0;
        vkEnumeratePhysicalDevices(ctx.vk_instance_, &device_count, nullptr);

        if (device_count == 0)
        {
            return callable::Chainable<CommVkPhysicalDeviceContext>(
                callable::error<CommVkPhysicalDeviceContext>("No physical devices found"));
        }

        std::vector<VkPhysicalDevice> devices(device_count);
        vkEnumeratePhysicalDevices(ctx.vk_instance_, &device_count, devices.data());

        // Create a scoring function for devices
        auto score_device =
            [&ctx](VkPhysicalDevice device) -> std::optional<std::pair<int, CommVkPhysicalDeviceContext>>
        {
            VkPhysicalDeviceProperties properties;
            VkPhysicalDeviceFeatures features;
            VkPhysicalDeviceMemoryProperties memory_props;

            vkGetPhysicalDeviceProperties(device, &properties);
            vkGetPhysicalDeviceFeatures(device, &features);
            vkGetPhysicalDeviceMemoryProperties(device, &memory_props);

            // Get queue family properties
            uint32_t queue_family_count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
            std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

            // Get available extensions
            uint32_t extension_count = 0;
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);
            std::vector<VkExtensionProperties> extensions(extension_count);
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, extensions.data());

            // Check basic requirements
            auto meets_api_version = [&]()
            {
                return !ctx.selection_criteria_.minimum_api_version_.has_value() ||
                       properties.apiVersion >= ctx.selection_criteria_.minimum_api_version_.value();
            };

            auto meets_extension_requirements = [&]()
            {
                return std::ranges::all_of(ctx.selection_criteria_.required_extensions_,
                                           [&extensions](const char* required_ext)
                                           {
                                               return std::ranges::any_of(extensions,
                                                                          [required_ext](const auto& available_ext) {
                                                                              return std::string_view(required_ext) ==
                                                                                     std::string_view(
                                                                                         available_ext.extensionName);
                                                                          });
                                           });
            };

            auto meets_queue_requirements = [&]()
            {
                return std::ranges::all_of(
                    ctx.selection_criteria_.queue_requirements_,
                    [&](const auto& queue_req)
                    {
                        return std::ranges::any_of(
                            std::views::iota(0U, static_cast<unsigned int>(queue_families.size())),
                            [&](unsigned int idx)
                            {
                                const auto& family = queue_families[idx];
                                bool flags_match =
                                    (family.queueFlags & queue_req.queue_flags_) == queue_req.queue_flags_;
                                bool count_sufficient = family.queueCount >= queue_req.min_queue_count_;

                                if (!flags_match || !count_sufficient)
                                    return false;

                                if (queue_req.require_present_support_ &&
                                    ctx.selection_criteria_.surface_ != VK_NULL_HANDLE)
                                {
                                    VkBool32 present_support = false;
                                    vkGetPhysicalDeviceSurfaceSupportKHR(device,
                                                                         idx,
                                                                         ctx.selection_criteria_.surface_,
                                                                         &present_support);
                                    return present_support == VK_TRUE;
                                }
                                return true;
                            });
                    });
            };

            // Check all requirements
            if (!meets_api_version() || !meets_extension_requirements() || !meets_queue_requirements())
            {
                return std::nullopt;
            }

            // Calculate score
            int score = 0;

            // Device type score
            if (ctx.selection_criteria_.prefer_discrete_gpu_ &&
                properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            {
                score += 1000;
            }
            else if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
            {
                score += 500;
            }

            // Memory score using ranges
            auto device_memory_heaps =
                std::views::iota(0U, memory_props.memoryHeapCount) 
                | std::views::transform([&memory_props](uint32_t idx) 
                { 
                    // Ensure index is within bounds
                    assert(idx < VK_MAX_MEMORY_HEAPS && "Memory heap index out of bounds");
                    return memory_props.memoryHeaps[idx]; 
                }) 
                | std::views::filter([](const auto& heap) { return heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT; });

            VkDeviceSize total_memory = std::accumulate(
                device_memory_heaps.begin(),
                device_memory_heaps.end(),
                VkDeviceSize{0},
                [](VkDeviceSize sum, const auto& heap) { return sum + heap.size; });

            score += static_cast<int>(total_memory / (static_cast<VkDeviceSize>(1024 * 1024)));

            // Create context copy with updated data
            CommVkPhysicalDeviceContext result_ctx = ctx;
            result_ctx.vk_physical_device_         = device;
            result_ctx.device_properties_          = properties;
            result_ctx.device_features_            = features;
            result_ctx.memory_properties_          = memory_props;
            result_ctx.queue_family_properties_    = queue_families;
            result_ctx.available_extensions_       = extensions;

            return std::make_pair(score, std::move(result_ctx));
        };

        // Score all devices and find the best one
        auto scored_devices = devices 
                            | std::views::transform(score_device) 
                            | std::views::filter([](const auto& opt) { return opt.has_value(); }) 
                            | std::views::transform([](const auto& opt) { return opt.value(); });

        auto best_device_it = std::ranges::max_element(
            scored_devices, [](const auto& left, const auto& right) { return left.first < right.first; });

        if (best_device_it == scored_devices.end())
        {
            return callable::Chainable<CommVkPhysicalDeviceContext>(
                callable::error<CommVkPhysicalDeviceContext>("No suitable physical device found"));
        }

        ctx = std::move((*best_device_it).second);
        if (ctx.vk_physical_device_ == VK_NULL_HANDLE)
        {
            return callable::Chainable<CommVkPhysicalDeviceContext>(
                callable::error<CommVkPhysicalDeviceContext>("No suitable physical device found"));
        }
        return callable::make_chain(std::move(ctx));
    };
}

} // namespace physicaldevice

} // namespace templates::common

#endif // COMMON_H