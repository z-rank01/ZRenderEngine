#ifndef COMMON_H
#define COMMON_H

#include "_callable/callable.h"
#include <stdexcept>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace templates::common {

/// @brief Vulkan instance context with lazy evaluation support
struct CommVkInstanceContext {
  // application info
  struct ApplicationInfo {
    std::string application_name_ = "Vulkan Engine";
    std::string engine_name_ = "Vulkan Engine";
    uint32_t application_version_ = VK_MAKE_VERSION(1, 0, 0);
    uint32_t engine_version_ = VK_MAKE_VERSION(1, 0, 0);
    uint32_t highest_api_version_ = VK_API_VERSION_1_3;
    void *p_next_ = nullptr;
  } app_info_;

  // instance info
  struct InstanceInfo {
    ApplicationInfo app_info_;
    std::vector<const char *> required_layers_;
    std::vector<const char *> required_extensions_;
  } instance_info_;

  // vulkan natives
  VkInstance vk_instance_ = VK_NULL_HANDLE;

  // Helper method to sync app info
  void sync_app_info() {
    instance_info_.app_info_ = app_info_;
  }
};

/// @brief Instance functions using Monad-Like Chain
namespace instance {

/// @brief Creates initial context
inline auto create_context() {
  return chainable::make_chain(CommVkInstanceContext{});
}

/// @brief Sets application name
inline auto set_application_name(const std::string &name) {
  return [name](CommVkInstanceContext ctx) -> chainable::Chainable<CommVkInstanceContext> {
    ctx.app_info_.application_name_ = name;
    ctx.sync_app_info();
    return chainable::make_chain(std::move(ctx));
  };
}

/// @brief Sets engine name
inline auto set_engine_name(const std::string &name) {
  return [name](CommVkInstanceContext ctx) -> chainable::Chainable<CommVkInstanceContext> {
    ctx.app_info_.engine_name_ = name;
    ctx.sync_app_info();
    return chainable::make_chain(std::move(ctx));
  };
}

/// @brief Sets application version
inline auto set_application_version(uint32_t major, uint32_t minor,
                                    uint32_t patch) {
  return [major, minor, patch](CommVkInstanceContext ctx) -> chainable::Chainable<CommVkInstanceContext> {
    ctx.app_info_.application_version_ = VK_MAKE_VERSION(major, minor, patch);
    ctx.sync_app_info();
    return chainable::make_chain(std::move(ctx));
  };
}

/// @brief Sets engine version
inline auto set_engine_version(uint32_t major, uint32_t minor, uint32_t patch) {
  return [major, minor, patch](CommVkInstanceContext ctx) -> chainable::Chainable<CommVkInstanceContext> {
    ctx.app_info_.engine_version_ = VK_MAKE_VERSION(major, minor, patch);
    ctx.sync_app_info();
    return chainable::make_chain(std::move(ctx));
  };
}

/// @brief Sets API version
inline auto set_api_version(uint32_t api_version) {
  return [api_version](CommVkInstanceContext ctx) -> chainable::Chainable<CommVkInstanceContext> {
    ctx.app_info_.highest_api_version_ = api_version;
    ctx.sync_app_info();
    return chainable::make_chain(std::move(ctx));
  };
}

/// @brief Adds validation layers
inline auto add_validation_layers(const std::vector<const char *> &layers) {
  return [layers](CommVkInstanceContext ctx) -> chainable::Chainable<CommVkInstanceContext> {
    ctx.instance_info_.required_layers_.insert(
        ctx.instance_info_.required_layers_.end(),
        layers.begin(),
        layers.end());
    return chainable::make_chain(std::move(ctx));
  };
}

/// @brief Adds extensions
inline auto add_extensions(const std::vector<const char *> &extensions) {
  return [extensions](CommVkInstanceContext ctx) -> chainable::Chainable<CommVkInstanceContext> {
    ctx.instance_info_.required_extensions_.insert(
        ctx.instance_info_.required_extensions_.end(),
        extensions.begin(),
        extensions.end());
    return chainable::make_chain(std::move(ctx));
  };
}

/// @brief Creates the Vulkan instance (final step)
inline auto create_vk_instance() {
  return [](CommVkInstanceContext ctx) -> chainable::Chainable<CommVkInstanceContext> {
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pNext = ctx.app_info_.p_next_;
    app_info.pApplicationName = ctx.app_info_.application_name_.c_str();
    app_info.applicationVersion = ctx.app_info_.application_version_;
    app_info.pEngineName = ctx.app_info_.engine_name_.c_str();
    app_info.engineVersion = ctx.app_info_.engine_version_;
    app_info.apiVersion = ctx.app_info_.highest_api_version_;

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledLayerCount =
        static_cast<uint32_t>(ctx.instance_info_.required_layers_.size());
    create_info.ppEnabledLayerNames =
        ctx.instance_info_.required_layers_.empty()
            ? nullptr
            : ctx.instance_info_.required_layers_.data();
    create_info.enabledExtensionCount = static_cast<uint32_t>(
        ctx.instance_info_.required_extensions_.size());
    create_info.ppEnabledExtensionNames =
        ctx.instance_info_.required_extensions_.empty()
            ? nullptr
            : ctx.instance_info_.required_extensions_.data();

    VkResult result =
        vkCreateInstance(&create_info, nullptr, &ctx.vk_instance_);
    if (result != VK_SUCCESS) {
      throw std::runtime_error(
          "Failed to create Vulkan instance. Error code: " +
          std::to_string(result));
    }

    return chainable::make_chain(std::move(ctx));
  };
}

/// @brief Validates context before creation
inline auto validate_context() {
  return [](CommVkInstanceContext ctx) -> chainable::Chainable<CommVkInstanceContext> {
    if (ctx.app_info_.application_name_.empty()) {
      return chainable::Chainable<CommVkInstanceContext>(
          chainable::error<CommVkInstanceContext>("Application name cannot be empty"));
    }
    if (ctx.app_info_.engine_name_.empty()) {
      return chainable::Chainable<CommVkInstanceContext>(
          chainable::error<CommVkInstanceContext>("Engine name cannot be empty"));
    }
    return chainable::make_chain(std::move(ctx));
  };
}

} // namespace instance

} // namespace templates::common

#endif // COMMON_H