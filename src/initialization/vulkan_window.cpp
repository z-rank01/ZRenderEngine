#include "vulkan_window.h"

// ------------------------------------
// VulkanSDLWindowHelper implementation
// ------------------------------------

VulkanSDLWindowHelper::~VulkanSDLWindowHelper()
{
    SDL_DestroyWindow(window_);
    SDL_Quit();
    SDL_free(extensions_);   // Free the allocated memory for extensions after it's copied or used by the helper

    vkDestroySurfaceKHR(*vk_instance_, surface_, nullptr);
}


VulkanSDLWindowHelper::VulkanSDLWindowHelper(SVulkanSDLWindowConfig config)
{
    // init sdl
    SDL_Init(config.init_flags_);

    // create sdl window
    window_ = SDL_CreateWindow(config.window_name_, config.width_, config.height_, config.window_flags_);
    window_extension_names_ = SDL_Vulkan_GetInstanceExtensions(&window_extension_count_);

    // show window (has implicitly created after SDL_CreateWindow)
    SDL_ShowWindow(window_);

    // generate window extensions
    int extension_count = window_extension_count_ + 1; // +1 for VK_EXT_DEBUG_REPORT_EXTENSION_NAME
    extensions_ = static_cast<const char**>(SDL_malloc(extension_count * sizeof(const char*)));
    if (!extensions_) {
        Logger::LogError("Failed to allocate memory for Vulkan extensions.");
        // Handle allocation failure, maybe throw an exception or return
        return;
    }
    extensions_[0] = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
    // Copy SDL extensions starting from index 1
    SDL_memcpy(&extensions_[1], window_extension_names_, window_extension_count_ * sizeof(const char*));
    window_extension_count_ = extension_count;
}

bool VulkanSDLWindowHelper::CreateSurface(const VkInstance* vkInstance)
{
    vk_instance_ = vkInstance;

    // create vulkan surface
    if (!SDL_Vulkan_CreateSurface(window_, *vk_instance_, nullptr, &surface_))
    {
        Logger::LogError("Failed to create Vulkan surface: " + std::string(SDL_GetError()));
        return false;
    }
    else
    {
        Logger::LogDebug("Succeeded in creating Vulkan surface.");
        return true;
    }
}

// ------------------------------------
// VulkanSwapChainHelper implementation
// ------------------------------------

VulkanSwapChainHelper::VulkanSwapChainHelper()
{
}

VulkanSwapChainHelper::~VulkanSwapChainHelper()
{
    vkDestroySwapchainKHR(*device_, swap_chain_, nullptr);
    for (const auto& image_view : swap_chain_image_views_)
    {
        vkDestroyImageView(*device_, image_view, nullptr);
    }
}

void VulkanSwapChainHelper::Setup(SVulkanSwapChainConfig config, const VkDevice* device, const VkPhysicalDevice* physical_device, const VkSurfaceKHR* surface, SDL_Window* window)
{
    swap_chain_config_ = config; // user config
    device_ = device;
    physical_device_ = physical_device;
    surface_ = surface;
    window_ = window;
}

bool VulkanSwapChainHelper::CreateSwapChain()
{
    // get swap chain support
    GetSwapChainSupport();

    // check surface format
    if (!CheckSurfaceFormat())
    {
        Logger::LogError("Surface format not supported.");
        return false;
    }

    // check present mode
    if (!CheckPresentMode())
    {
        Logger::LogError("Present mode not supported.");
        return false;
    }

    // check swap extent
    if (!CheckSwapExtent())
    {
        Logger::LogError("Swap extent not supported.");
        return false;
    }

    // create swap chain
    CreateSwapChainInternal();

    // create image views
    CreateImageViews();

    return true;
}

void VulkanSwapChainHelper::CheckExtensions()
{
    // check extensions
    uint32_t extension_count = 0;
    vkEnumerateDeviceExtensionProperties(*physical_device_, nullptr, &extension_count, nullptr);
    std::vector<VkExtensionProperties> extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(*physical_device_, nullptr, &extension_count, extensions.data());

    std::unordered_set<std::string> supported_extensions(extension_count);
    std::vector<const char*> available_extensions;
    for (const auto& extension : extensions)
    {
        supported_extensions.insert(extension.extensionName);
    }
    for (const auto& extension : swap_chain_config_.device_extensions_)
    {
        if (supported_extensions.find(extension) != supported_extensions.end())
        {
            available_extensions.push_back(extension);
        }
        else
        {
            Logger::LogWarning("Required device extension not supported: " + std::string(extension));
        }
    }

    // set the available extensions
    swap_chain_config_.device_extensions_ = available_extensions;
}

void VulkanSwapChainHelper::GetSwapChainSupport()
{
    // get swap chain support
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(*physical_device_, *surface_, &surface_capabilities_);

    // get surface formats
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(*physical_device_, *surface_, &format_count, nullptr);
    if (format_count != 0)
    {
        surface_formats_.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(*physical_device_, *surface_, &format_count, surface_formats_.data());
    }

    // get present modes
    uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(*physical_device_, *surface_, &present_mode_count, nullptr);
    if (present_mode_count != 0)
    {
        present_modes_.resize(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(*physical_device_, *surface_, &present_mode_count, present_modes_.data());
    }
}

bool VulkanSwapChainHelper::CheckSurfaceFormat()
{
    // pick surface format
    if (surface_formats_.size() == 1 && surface_formats_[0].format == VK_FORMAT_UNDEFINED)
    {
        swap_chain_config_.target_surface_format_ = surface_formats_[0];
    }
    else
    {
        for (const auto& format : surface_formats_)
        {
            if (format.format == swap_chain_config_.target_surface_format_.format && 
                format.colorSpace == swap_chain_config_.target_surface_format_.colorSpace)
            {
                return true;
            }
        }
    }
    return false;
}

bool VulkanSwapChainHelper::CheckPresentMode()
{
    // pick present mode
    if (present_modes_.size() == 1 && present_modes_[0] == VK_PRESENT_MODE_IMMEDIATE_KHR)
    {
        swap_chain_config_.target_present_mode_ = present_modes_[0];
    }
    else
    {
        for (const auto& mode : present_modes_)
        {
            if (mode == swap_chain_config_.target_present_mode_)
            {
                return true;
            }
        }
    }
    return false;
}

bool VulkanSwapChainHelper::CheckSwapExtent()
{
    // pick swap extent
    int current_width = 0;
    int current_height = 0;
    if (!SDL_GetWindowSizeInPixels(window_, &current_width, &current_height))
    {
        Logger::LogError("Failed to get window size: " + std::string(SDL_GetError()));
        return false;
    }

    // set the swap extent
    VkExtent2D actualExtent = { current_width, current_height };
    actualExtent.width = std::clamp(actualExtent.width, surface_capabilities_.minImageExtent.width, surface_capabilities_.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height, surface_capabilities_.minImageExtent.height, surface_capabilities_.maxImageExtent.height);
    swap_chain_config_.target_swap_extent_ = actualExtent;

    return true;
}

void VulkanSwapChainHelper::CreateSwapChainInternal()
{
    // create swap chain

    VkSwapchainCreateInfoKHR swap_chain_info = {};
    swap_chain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swap_chain_info.surface = *surface_;
    swap_chain_info.minImageCount = swap_chain_config_.target_image_count_;
    swap_chain_info.imageFormat = swap_chain_config_.target_surface_format_.format;
    swap_chain_info.imageColorSpace = swap_chain_config_.target_surface_format_.colorSpace;
    swap_chain_info.imageExtent = swap_chain_config_.target_swap_extent_;
    swap_chain_info.imageArrayLayers = 1;
    swap_chain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swap_chain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swap_chain_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swap_chain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swap_chain_info.presentMode = swap_chain_config_.target_present_mode_;
    swap_chain_info.clipped = VK_TRUE;
    swap_chain_info.oldSwapchain = VK_NULL_HANDLE;
    swap_chain_info.pNext = nullptr;
    swap_chain_info.flags = 0;
    if (!Logger::LogWithVkResult(
        vkCreateSwapchainKHR(*device_, &swap_chain_info, nullptr, &swap_chain_),
        "Failed to create swap chain",
        "Succeeded in creating swap chain"))
    {
        Logger::LogError("Failed to create swap chain.");
        return;
    }
    else
    {
        Logger::LogDebug("Succeeded in creating swap chain.");
    }
}

void VulkanSwapChainHelper::CreateImageViews()
{
    // get swap chain images
    uint32_t image_count = 0;
    vkGetSwapchainImagesKHR(*device_, swap_chain_, &image_count, nullptr);
    swap_chain_images_.resize(image_count);
    vkGetSwapchainImagesKHR(*device_, swap_chain_, &image_count, swap_chain_images_.data());
    swap_chain_image_views_.resize(image_count);
    for (size_t i = 0; i < image_count; ++i)
    {
        VkImageViewCreateInfo view_info = {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = swap_chain_images_[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = swap_chain_config_.target_surface_format_.format;
        view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        Logger::LogWithVkResult(
            vkCreateImageView(*device_, &view_info, nullptr, &swap_chain_image_views_[i]),
            "Failed to create swapchain image view",
            "Succeeded in creating swapchain image view");
    }
}
