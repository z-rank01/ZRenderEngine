#include "vulkan_window.h"

VulkanSDLWindowHelper::~VulkanSDLWindowHelper()
{
    SDL_DestroyWindow(window_);
    SDL_Quit();
    SDL_free(extensions_);   // Free the allocated memory for extensions after it's copied or used by the helper

    vkDestroySurfaceKHR(*vk_instance_, surface_, nullptr);
}


VulkanSDLWindowHelper::VulkanSDLWindowHelper(SVulkanSDLWindowConfig config)
{
    window_config_ = config;

    // init sdl
    SDL_Init(config.init_flags_);
    
    // create sdl window
    window_ = SDL_CreateWindow(config.window_name_, config.width_, config.height_, config.window_flags_);
    window_extension_names_ = SDL_Vulkan_GetInstanceExtensions(&window_extension_count_);

    // show window (has implicitly created after SDL_CreateWindow)
    SDL_ShowWindow(window_);

    // generate window extensions
    int extension_count = window_extension_count_ + 1; // +1 for VK_EXT_DEBUG_REPORT_EXTENSION_NAME
    extensions_ = static_cast<const char**>(SDL_malloc(extension_count * sizeof(const char *)));
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

void VulkanSDLWindowHelper::CreateSurface(const VkInstance* vkInstance)
{
    vk_instance_ = vkInstance;
    
    // create vulkan surface
    if (!SDL_Vulkan_CreateSurface(window_, *vk_instance_, nullptr, &surface_))
    {
        Logger::LogError("Failed to create Vulkan surface: " + std::string(SDL_GetError()));
        return;
    }
    else
    {
        Logger::LogDebug("Succeeded in creating Vulkan surface.");
    }
}