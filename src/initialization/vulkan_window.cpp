#include "vulkan_window.h"
#include <vector> // 确保包含 vector

// ------------------------------------
// VulkanSDLWindowHelper implementation
// ------------------------------------

VulkanSDLWindowHelper::~VulkanSDLWindowHelper()
{
    // 释放深拷贝的字符串
    for (const char* ext : extensions_) {
        SDL_free((void*)ext); // 强制转换是必要的，因为 SDL_free 需要 void*
    }
    extensions_.clear(); // 清空向量

    if (surface_ != VK_NULL_HANDLE && vk_instance_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(vk_instance_, surface_, nullptr);
    }
    
    if (window_) {
        SDL_DestroyWindow(window_);
    }
    SDL_Quit();
}


VulkanSDLWindowHelper::VulkanSDLWindowHelper(SVulkanSDLWindowConfig config)
    : window_(nullptr), sdl_extension_count_(0), window_extension_names_(nullptr), vk_instance_(VK_NULL_HANDLE), surface_(VK_NULL_HANDLE)
{
    // init sdl
    if (!SDL_Init(config.init_flags_)) {
        throw "Failed to initialize SDL: " + std::string(SDL_GetError());
    }

    // create sdl window
    window_ = SDL_CreateWindow(config.window_name_, config.width_, config.height_, config.window_flags_);
    if (!window_) {
        throw "Failed to create SDL window: " + std::string(SDL_GetError());
        SDL_Quit();
    }

    // 获取 SDL 提供的 Vulkan 实例扩展
    window_extension_names_ = SDL_Vulkan_GetInstanceExtensions(&sdl_extension_count_);
    if (!window_extension_names_) {
         throw "SDL_Vulkan_GetInstanceExtensions failed: " + std::string(SDL_GetError());
         // 继续执行，可能没有额外的扩展
         sdl_extension_count_ = 0; 
    }

    // generate window extensions vector
    extensions_.clear();
    extensions_.reserve(sdl_extension_count_ + 1); 

    // debug utility extension
    auto debug_ext = SDL_strdup(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    if (!debug_ext) throw "Failed to add debug extension.";
    extensions_.push_back(debug_ext);

    // add sdl required extensions
    for (unsigned int i = 0; i < sdl_extension_count_; ++i) {
        auto ext_copy = SDL_strdup(window_extension_names_[i]);
        if (!ext_copy) throw "Failed to add SDL extension: " + std::string(window_extension_names_[i]);
        extensions_.push_back(ext_copy);
    }
}

bool VulkanSDLWindowHelper::CreateSurface(VkInstance vkInstance)
{
    if (!window_) {
        Logger::LogError("Cannot create surface without a valid SDL window.");
        return false;
    }
    vk_instance_ = vkInstance;

    // create vulkan surface
    if (!SDL_Vulkan_CreateSurface(window_, vk_instance_, nullptr, &surface_))
    {
        Logger::LogError("Failed to create Vulkan surface: " + std::string(SDL_GetError()));
        surface_ = VK_NULL_HANDLE;
        return false;
    }
    else
    {
        Logger::LogDebug("Succeeded in creating Vulkan surface.");
        return true;
    }
}

VkExtent2D VulkanSDLWindowHelper::GetCurrentWindowExtent() const
{
    int width, height;
    SDL_GetWindowSizeInPixels(window_, &width, &height);
    return { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
}

// ------------------------------------
// VulkanSwapChainHelper implementation
// ------------------------------------

VulkanSwapChainHelper::VulkanSwapChainHelper()
{
}

VulkanSwapChainHelper::~VulkanSwapChainHelper()
{
    vkDestroySwapchainKHR(device_, swap_chain_, nullptr);
    for (const auto& image_view : swap_chain_image_views_)
    {
        vkDestroyImageView(device_, image_view, nullptr);
    }
}

void VulkanSwapChainHelper::Setup(SVulkanSwapChainConfig config, VkDevice device, VkPhysicalDevice physical_device, VkSurfaceKHR surface, SDL_Window* window)
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

bool VulkanSwapChainHelper::AcquireNextImage(uint32_t& image_index, VkSemaphore semaphore, VkFence fence)
{
    return Logger::LogWithVkResult(
        vkAcquireNextImageKHR(device_, swap_chain_, UINT64_MAX, semaphore, fence, &image_index),
        "Failed to acquire next image",
        "Succeeded in acquiring next image");
}

bool VulkanSwapChainHelper::AcquireNextImage(uint32_t& image_index, bool& resize_request, VkSemaphore semaphore, VkFence fence)
{
    VkResult result = vkAcquireNextImageKHR(device_, swap_chain_, UINT64_MAX, semaphore, fence, &image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        resize_request = true;
        return false;
    }
    else if (result != VK_SUCCESS)
    {
        Logger::LogError("Failed to acquire next image: " + std::to_string(result));
        return false;
    }
    return true;
}

bool VulkanSwapChainHelper::DestroySwapChain()
{
    vkDestroySwapchainKHR(device_, swap_chain_, nullptr);
    for (auto image_view : swap_chain_image_views_)
    {
        vkDestroyImageView(device_, image_view, nullptr);
    }
    swap_chain_image_views_.clear();
    swap_chain_images_.clear();
    return true;
}

void VulkanSwapChainHelper::CheckExtensions()
{
    // check extensions
    uint32_t extension_count = 0;
    vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &extension_count, nullptr);
    std::vector<VkExtensionProperties> extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &extension_count, extensions.data());

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
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, surface_, &surface_capabilities_);

    // get surface formats
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &format_count, nullptr);
    if (format_count != 0)
    {
        surface_formats_.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &format_count, surface_formats_.data());
    }

    // get present modes
    uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_, &present_mode_count, nullptr);
    if (present_mode_count != 0)
    {
        present_modes_.resize(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_, &present_mode_count, present_modes_.data());
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
    swap_chain_info.surface = surface_;
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
        vkCreateSwapchainKHR(device_, &swap_chain_info, nullptr, &swap_chain_),
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
    vkGetSwapchainImagesKHR(device_, swap_chain_, &image_count, nullptr);
    swap_chain_images_.resize(image_count);
    vkGetSwapchainImagesKHR(device_, swap_chain_, &image_count, swap_chain_images_.data());
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
            vkCreateImageView(device_, &view_info, nullptr, &swap_chain_image_views_[i]),
            "Failed to create swapchain image view",
            "Succeeded in creating swapchain image view");
    }
}
