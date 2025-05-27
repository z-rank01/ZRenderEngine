#ifndef COMMON_H
#define COMMON_H

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <numeric>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
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
        const auto kMissingExtensions = std::ranges::find_if(
            ctx.selection_criteria_.required_extensions_,
            [&ctx](const char* required_ext)
            {
                return !std::ranges::any_of(
                    ctx.available_extensions_,
                    [required_ext](const auto& available_ext)
                    { return std::string_view(required_ext) == std::string_view(available_ext.extensionName); });
            });

        if (kMissingExtensions != std::end(ctx.selection_criteria_.required_extensions_))
        {
            return callable::Chainable<CommVkPhysicalDeviceContext>(callable::error<CommVkPhysicalDeviceContext>(
                std::string("Required extension not available: ") + *kMissingExtensions));
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
                                    vkGetPhysicalDeviceSurfaceSupportKHR(
                                        device, idx, ctx.selection_criteria_.surface_, &present_support);
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
                std::views::iota(0U, memory_props.memoryHeapCount) |
                std::views::transform(
                    [&memory_props](uint32_t idx)
                    {
                        // Ensure index is within bounds
                        assert(idx < VK_MAX_MEMORY_HEAPS && "Memory heap index out of bounds");
                        return memory_props.memoryHeaps[idx];
                    }) |
                std::views::filter([](const auto& heap) { return heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT; });

            VkDeviceSize total_memory =
                std::accumulate(device_memory_heaps.begin(),
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
        auto scored_devices = devices | std::views::transform(score_device) |
                              std::views::filter([](const auto& opt) { return opt.has_value(); }) |
                              std::views::transform([](const auto& opt) { return opt.value(); });

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

/// -------------------------------------------------
/// vulkan logical device templated functions: common
/// -------------------------------------------------

/// @brief Vulkan logical device context with lazy evaluation support
struct CommVkLogicalDeviceContext
{
    // parent physical device context
    VkPhysicalDevice vk_physical_device_ = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties device_properties_{};
    VkPhysicalDeviceFeatures device_features_{};
    std::vector<VkQueueFamilyProperties> queue_family_properties_;

    // logical device creation info
    struct DeviceInfo
    {
        std::vector<VkDeviceQueueCreateInfo> queue_create_infos_;
        std::vector<const char*> required_extensions_;
        VkPhysicalDeviceFeatures required_features_{};
        VkPhysicalDeviceVulkan11Features required_features_11_{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
        VkPhysicalDeviceVulkan12Features required_features_12_{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        VkPhysicalDeviceVulkan13Features required_features_13_{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        void* p_next_ = nullptr;
    } device_info_;

    // queue configuration
    struct QueueInfo
    {
        uint32_t queue_family_index_;
        uint32_t queue_count_;
        std::vector<float> queue_priorities_;
        VkQueueFlags queue_flags_;
        std::string queue_name_; // for identification
    };
    std::vector<QueueInfo> queue_infos_;

    // vulkan natives
    VkDevice vk_logical_device_ = VK_NULL_HANDLE;
    std::unordered_map<std::string, VkQueue> named_queues_;
    std::unordered_map<uint32_t, std::vector<VkQueue>> family_queues_;
};

namespace logicaldevice
{

/// @brief Creates initial logical device context from physical device context
inline auto create_logical_device_context(const CommVkPhysicalDeviceContext& physical_device_ctx)
{
    return callable::make_chain(
        [physical_device_ctx]() -> CommVkLogicalDeviceContext
        {
            CommVkLogicalDeviceContext ctx;
            ctx.vk_physical_device_      = physical_device_ctx.vk_physical_device_;
            ctx.device_properties_       = physical_device_ctx.device_properties_;
            ctx.device_features_         = physical_device_ctx.device_features_;
            ctx.queue_family_properties_ = physical_device_ctx.queue_family_properties_;
            return ctx;
        }());
}

/// @brief Adds required device extensions
inline auto require_extensions(const std::vector<const char*>& extensions)
{
    return [extensions](CommVkLogicalDeviceContext ctx) -> callable::Chainable<CommVkLogicalDeviceContext>
    {
        ctx.device_info_.required_extensions_.insert(
            ctx.device_info_.required_extensions_.end(), extensions.begin(), extensions.end());
        return callable::make_chain(std::move(ctx));
    };
}

/// @brief Sets required Vulkan 1.0 features
inline auto require_features(const VkPhysicalDeviceFeatures& features)
{
    return [features](CommVkLogicalDeviceContext ctx) -> callable::Chainable<CommVkLogicalDeviceContext>
    {
        ctx.device_info_.required_features_ = features;
        return callable::make_chain(std::move(ctx));
    };
}

/// @brief Sets required Vulkan 1.1 features
inline auto require_features_11(const VkPhysicalDeviceVulkan11Features& features)
{
    return [features](CommVkLogicalDeviceContext ctx) -> callable::Chainable<CommVkLogicalDeviceContext>
    {
        ctx.device_info_.required_features_11_ = features;
        return callable::make_chain(std::move(ctx));
    };
}

/// @brief Sets required Vulkan 1.2 features
inline auto require_features_12(const VkPhysicalDeviceVulkan12Features& features)
{
    return [features](CommVkLogicalDeviceContext ctx) -> callable::Chainable<CommVkLogicalDeviceContext>
    {
        ctx.device_info_.required_features_12_ = features;
        return callable::make_chain(std::move(ctx));
    };
}

/// @brief Sets required Vulkan 1.3 features
inline auto require_features_13(const VkPhysicalDeviceVulkan13Features& features)
{
    return [features](CommVkLogicalDeviceContext ctx) -> callable::Chainable<CommVkLogicalDeviceContext>
    {
        ctx.device_info_.required_features_13_ = features;
        return callable::make_chain(std::move(ctx));
    };
}

/// @brief Adds a queue request with name for easy identification
inline auto add_queue(const std::string& queue_name,
                      uint32_t queue_family_index,
                      uint32_t queue_count                 = 1,
                      const std::vector<float>& priorities = {1.0f})
{
    return [queue_name, queue_family_index, queue_count, priorities](
               CommVkLogicalDeviceContext ctx) -> callable::Chainable<CommVkLogicalDeviceContext>
    {
        // Validate queue family index
        if (queue_family_index >= ctx.queue_family_properties_.size())
        {
            return callable::Chainable<CommVkLogicalDeviceContext>(
                callable::error<CommVkLogicalDeviceContext>("Invalid queue family index"));
        }

        // Validate queue count
        if (queue_count > ctx.queue_family_properties_[queue_family_index].queueCount)
        {
            return callable::Chainable<CommVkLogicalDeviceContext>(
                callable::error<CommVkLogicalDeviceContext>("Requested queue count exceeds available queues"));
        }

        // Create queue info
        CommVkLogicalDeviceContext::QueueInfo queue_info;
        queue_info.queue_family_index_ = queue_family_index;
        queue_info.queue_count_        = queue_count;
        queue_info.queue_priorities_   = priorities.empty() ? std::vector<float>(queue_count, 1.0F) : priorities;
        queue_info.queue_flags_        = ctx.queue_family_properties_[queue_family_index].queueFlags;
        queue_info.queue_name_         = queue_name;

        // Ensure priorities match queue count
        if (queue_info.queue_priorities_.size() != queue_count)
        {
            queue_info.queue_priorities_.resize(queue_count, 1.0F);
        }

        ctx.queue_infos_.push_back(queue_info);
        return callable::make_chain(std::move(ctx));
    };
}

/// @brief Adds a graphics queue automatically finding suitable family
inline auto add_graphics_queue(const std::string& queue_name = "graphics",
                               VkSurfaceKHR surface          = VK_NULL_HANDLE,
                               uint32_t queue_count          = 1)
{
    return [queue_name, surface, queue_count](
               CommVkLogicalDeviceContext ctx) -> callable::Chainable<CommVkLogicalDeviceContext>
    {
        // Find graphics queue family
        std::optional<uint32_t> graphics_family;

        auto graphics_families =
            std::views::iota(0U, static_cast<uint32_t>(ctx.queue_family_properties_.size())) |
            std::views::filter(
                [&](uint32_t idx)
                {
                    const auto& family = ctx.queue_family_properties_[idx];

                    // Check for graphics support
                    if (!(family.queueFlags & VK_QUEUE_GRAPHICS_BIT))
                        return false;

                    // If surface provided, check for present support
                    if (surface != VK_NULL_HANDLE)
                    {
                        VkBool32 present_support = false;
                        vkGetPhysicalDeviceSurfaceSupportKHR(ctx.vk_physical_device_, idx, surface, &present_support);
                        return present_support == VK_TRUE;
                    }

                    return true;
                });

        auto first_suitable_graphics = graphics_families.begin();
        if (first_suitable_graphics != graphics_families.end()) {
            graphics_family = *first_suitable_graphics;
        }

        if (!graphics_family.has_value()) {
            return callable::Chainable<CommVkLogicalDeviceContext>(
                callable::error<CommVkLogicalDeviceContext>("No suitable graphics queue family found"));
        }

        return add_queue(queue_name, graphics_family.value(), queue_count)(std::move(ctx));
    };
}

/// @brief Adds a compute queue automatically finding suitable family
/// @param queue_name Name for the compute queue (default is "compute")
/// @param queue_count Number of compute queues to create (default is 1)
inline auto add_compute_queue(const std::string& queue_name = "compute", uint32_t queue_count = 1)
{
    return [queue_name, queue_count](CommVkLogicalDeviceContext ctx) -> callable::Chainable<CommVkLogicalDeviceContext>
    {
        // Use ranges::find_if find dedicated compute queue family first
        auto dedicated_compute = std::ranges::find_if(
            std::views::iota(0U, static_cast<uint32_t>(ctx.queue_family_properties_.size())),
            [&](uint32_t idx) {
                const auto& family = ctx.queue_family_properties_[idx];
                return (family.queueFlags & VK_QUEUE_COMPUTE_BIT) &&
                       !(family.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT));
            });
        
        // If any dedicated compute queue family found, use it
        if (dedicated_compute != std::ranges::end(
            std::views::iota(0U, static_cast<uint32_t>(ctx.queue_family_properties_.size()))))
        {
            return add_queue(queue_name, *dedicated_compute, queue_count)(std::move(ctx));
        }

        // Otherwise find graphics queue that supports compute
        auto compute_graphics = std::ranges::find_if(
            std::views::iota(0U, static_cast<uint32_t>(ctx.queue_family_properties_.size())),
            [&](uint32_t idx) {
                const auto& family = ctx.queue_family_properties_[idx];
                return (family.queueFlags & VK_QUEUE_COMPUTE_BIT) &&
                       (family.queueFlags & VK_QUEUE_GRAPHICS_BIT);
            });
        
        // If any compute-capable graphics queue family found, use it
        if (compute_graphics != std::ranges::end(
            std::views::iota(0U, static_cast<uint32_t>(ctx.queue_family_properties_.size()))))
        {
            return add_queue(queue_name, *compute_graphics, queue_count)(std::move(ctx));
        }
        
        // fallback
        return callable::Chainable<CommVkLogicalDeviceContext>(
            callable::error<CommVkLogicalDeviceContext>("No suitable compute queue family found"));
    };
}

/// @brief Adds a transfer queue automatically finding suitable family
/// @param queue_name Name for the transfer queue (default is "transfer")
/// @param queue_count Number of transfer queues to create (default is 1)
inline auto add_transfer_queue(const std::string& queue_name = "transfer", uint32_t queue_count = 1)
{
    return [queue_name, queue_count](CommVkLogicalDeviceContext ctx) -> callable::Chainable<CommVkLogicalDeviceContext>
    {
        // Use ranges::find_if find dedicated compute queue family first
        auto dedicated_transfer = std::ranges::find_if(
            std::views::iota(0U, static_cast<uint32_t>(ctx.queue_family_properties_.size())),
            [&](uint32_t idx) {
                const auto& family = ctx.queue_family_properties_[idx];
                return (family.queueFlags & VK_QUEUE_TRANSFER_BIT) &&
                       !(family.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT));
            });

        // If any dedicated transfer queue family found, use it
        if (dedicated_transfer != std::ranges::end(
            std::views::iota(0U, static_cast<uint32_t>(ctx.queue_family_properties_.size()))))
        {
            return add_queue(queue_name, *dedicated_transfer, queue_count)(std::move(ctx));
        }

        // Otherwise find graphics queue that supports transfer
        auto transfer_graphics = std::ranges::find_if(
            std::views::iota(0U, static_cast<uint32_t>(ctx.queue_family_properties_.size())),
            [&](uint32_t idx) {
                const auto& family = ctx.queue_family_properties_[idx];
                return (family.queueFlags & VK_QUEUE_TRANSFER_BIT) &&
                       (family.queueFlags & VK_QUEUE_GRAPHICS_BIT);
            });

        // If any transfer-capable graphics queue family found, use it
        if (transfer_graphics != std::ranges::end(
            std::views::iota(0U, static_cast<uint32_t>(ctx.queue_family_properties_.size()))))
        {
            return add_queue(queue_name, *transfer_graphics, queue_count)(std::move(ctx));
        }
        
        // fallback
        return callable::Chainable<CommVkLogicalDeviceContext>(
            callable::error<CommVkLogicalDeviceContext>("No suitable transfer queue family found"));
    };
}

/// @brief Validates device configuration
inline auto validate_device_configuration()
{
    return [](CommVkLogicalDeviceContext ctx) -> callable::Chainable<CommVkLogicalDeviceContext>
    {
        if (ctx.queue_infos_.empty())
        {
            return callable::Chainable<CommVkLogicalDeviceContext>(
                callable::error<CommVkLogicalDeviceContext>("No queues specified for device creation"));
        }

        // Check for duplicate queue names
        std::unordered_set<std::string> queue_names;
        for (const auto& queue_info : ctx.queue_infos_)
        {
            if (!queue_names.insert(queue_info.queue_name_).second)
            {
                return callable::Chainable<CommVkLogicalDeviceContext>(
                    callable::error<CommVkLogicalDeviceContext>("Duplicate queue name: " + queue_info.queue_name_));
            }
        }

        return callable::make_chain(std::move(ctx));
    };
}

/// @brief Creates the logical device and retrieves queues
inline auto create_logical_device()
{
    return [](CommVkLogicalDeviceContext ctx) -> callable::Chainable<CommVkLogicalDeviceContext>
    {
        // Consolidate queue create infos by family
        std::unordered_map<uint32_t, VkDeviceQueueCreateInfo> family_queue_infos;
        std::unordered_map<uint32_t, std::vector<float>> family_priorities;

        for (const auto& queue_info : ctx.queue_infos_)
        {
            uint32_t family_index = queue_info.queue_family_index_;

            if (family_queue_infos.find(family_index) == family_queue_infos.end())
            {
                // First queue for this family
                VkDeviceQueueCreateInfo queue_create_info{};
                queue_create_info.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queue_create_info.queueFamilyIndex = family_index;
                queue_create_info.queueCount       = queue_info.queue_count_;

                family_queue_infos[family_index] = queue_create_info;
                family_priorities[family_index]  = queue_info.queue_priorities_;
            }
            else
            {
                // Additional queues for existing family
                auto& existing_info   = family_queue_infos[family_index];
                auto& existing_priorities       = family_priorities[family_index];

                existing_info.queueCount += queue_info.queue_count_;
                existing_priorities.insert(existing_priorities.end(),
                                           queue_info.queue_priorities_.begin(),
                                           queue_info.queue_priorities_.end());
            }
        }

        // Update priority pointers
        for (auto& [family_index, queue_info] : family_queue_infos)
        {
            queue_info.pQueuePriorities = family_priorities[family_index].data();
        }

        // Convert to vector for device creation
        std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
        for (const auto& [family_index, queue_info] : family_queue_infos)
        {
            queue_create_infos.push_back(queue_info);
        }

        // Setup feature chain
        void* feature_chain = nullptr;
        if (ctx.device_info_.required_features_13_.sType != 0)
        {
            ctx.device_info_.required_features_13_.pNext = feature_chain;
            feature_chain                                = &ctx.device_info_.required_features_13_;
        }
        if (ctx.device_info_.required_features_12_.sType != 0)
        {
            ctx.device_info_.required_features_12_.pNext = feature_chain;
            feature_chain                                = &ctx.device_info_.required_features_12_;
        }
        if (ctx.device_info_.required_features_11_.sType != 0)
        {
            ctx.device_info_.required_features_11_.pNext = feature_chain;
            feature_chain                                = &ctx.device_info_.required_features_11_;
        }

        // Create device
        VkDeviceCreateInfo device_create_info{};
        device_create_info.sType                 = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_create_info.pNext                 = feature_chain;
        device_create_info.queueCreateInfoCount  = static_cast<uint32_t>(queue_create_infos.size());
        device_create_info.pQueueCreateInfos     = queue_create_infos.data();
        device_create_info.enabledExtensionCount = static_cast<uint32_t>(ctx.device_info_.required_extensions_.size());
        device_create_info.ppEnabledExtensionNames =
            ctx.device_info_.required_extensions_.empty() ? nullptr : ctx.device_info_.required_extensions_.data();
        device_create_info.pEnabledFeatures = &ctx.device_info_.required_features_;

        VkResult result =
            vkCreateDevice(ctx.vk_physical_device_, &device_create_info, nullptr, &ctx.vk_logical_device_);
        if (result != VK_SUCCESS)
        {
            return callable::Chainable<CommVkLogicalDeviceContext>(callable::error<CommVkLogicalDeviceContext>(
                "Failed to create logical device. Error: " + std::to_string(result)));
        }

        // Retrieve queues
        std::unordered_map<uint32_t, uint32_t> family_queue_counters;
        for (const auto& queue_info : ctx.queue_infos_)
        {
            uint32_t family_index = queue_info.queue_family_index_;
            uint32_t& counter     = family_queue_counters[family_index];

            for (uint32_t i = 0; i < queue_info.queue_count_; ++i)
            {
                VkQueue queue;
                vkGetDeviceQueue(ctx.vk_logical_device_, family_index, counter, &queue);

                // Store in named queues (use index suffix for multiple queues)
                std::string queue_name = queue_info.queue_name_;
                if (queue_info.queue_count_ > 1)
                {
                    queue_name += "_" + std::to_string(i);
                }
                ctx.named_queues_[queue_name] = queue;

                // Store in family queues
                ctx.family_queues_[family_index].push_back(queue);

                ++counter;
            }
        }

        return callable::make_chain(std::move(ctx));
    };
}

/// @brief Helper function to get queue by name
inline VkQueue get_queue(const CommVkLogicalDeviceContext& ctx, const std::string& queue_name)
{
    auto it = ctx.named_queues_.find(queue_name);
    return (it != ctx.named_queues_.end()) ? it->second : VK_NULL_HANDLE;
}

/// @brief Helper function to get queue family index by queue flags
inline std::optional<uint32_t> find_queue_family(const CommVkLogicalDeviceContext& ctx, VkQueueFlags queue_flags)
{
    for (uint32_t i = 0; i < ctx.queue_family_properties_.size(); i++)
    {
        if ((ctx.queue_family_properties_[i].queueFlags & queue_flags) == queue_flags)
        {
            return i;
        }
    }
    return std::nullopt;
}

/// @brief Helper function to get all queues from a family
inline std::vector<VkQueue> get_family_queues(const CommVkLogicalDeviceContext& ctx, uint32_t family_index)
{
    auto it = ctx.family_queues_.find(family_index);
    return (it != ctx.family_queues_.end()) ? it->second : std::vector<VkQueue>{};
}

} // namespace logicaldevice

} // namespace templates::common

#endif // COMMON_H
