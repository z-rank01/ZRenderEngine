#pragma once

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include "utility/logger.h"

struct SVulkanSDLWindowConfig
{
    const char* window_name_;
    int width_;
    int height_;
    SDL_WindowFlags window_flags_;
    SDL_InitFlags init_flags_;
};

class VulkanSDLWindowHelper
{
public:
    VulkanSDLWindowHelper() = delete;
    VulkanSDLWindowHelper(SVulkanSDLWindowConfig config);
    ~VulkanSDLWindowHelper();

    void CreateSurface(const VkInstance* vkInstance);
    const VkSurfaceKHR& GetSurface() const { return surface_; }
    
    const char** GetWindowExtensions() const { return extensions_; }
    int GetWindowExtensionCount() const { return window_extension_count_; }
    

private:
    SVulkanSDLWindowConfig window_config_;

    // sdl
    SDL_Window* window_;
    unsigned int window_extension_count_;
    const char* const* window_extension_names_;
    const char** extensions_;

    // vulkan
    const VkInstance* vk_instance_;
    VkSurfaceKHR surface_;
};