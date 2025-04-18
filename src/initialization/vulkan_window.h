#pragma once

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <string>
#include <unordered_set>
#include <algorithm>
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
    SDL_Window* GetWindow() const { return window_; }
    
    const char** GetWindowExtensions() const { return extensions_; }
    int GetWindowExtensionCount() const { return window_extension_count_; }
    

private:
    // sdl
    SDL_Window* window_;
    unsigned int window_extension_count_;
    const char* const* window_extension_names_;
    const char** extensions_;

    // vulkan
    const VkInstance* vk_instance_;
    VkSurfaceKHR surface_;
};

struct SVulkanSwapChainConfig
{
    VkSurfaceFormatKHR target_surface_format_;
    VkPresentModeKHR target_present_mode_;
    VkExtent2D target_swap_extent_;
    uint32_t target_image_count_;
    std::vector<const char*> device_extensions_;

    // Default constructor
    SVulkanSwapChainConfig() 
    {
        target_surface_format_.format = VK_FORMAT_B8G8R8A8_UNORM;
        target_surface_format_.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        target_present_mode_ = VK_PRESENT_MODE_FIFO_KHR;
        target_swap_extent_.width = 800;
        target_swap_extent_.height = 600;
        target_image_count_ = 2; // Double buffering
        device_extensions_.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }
};

class VulkanSwapChainHelper
{
public:
    VulkanSwapChainHelper();
    ~VulkanSwapChainHelper();

    void Setup(SVulkanSwapChainConfig config, const VkDevice& device, const VkPhysicalDevice& physical_device, const VkSurfaceKHR& surface, SDL_Window* window);
    bool CreateSwapChain();
    int GetSwapChainExtensions(const VkPhysicalDevice& physical_device, std::vector<const char*>& extensions) 
    {
        physical_device_ = &physical_device; // Set the physical device
        CheckExtensions();
        extensions = swap_chain_config_.device_extensions_; 
        return swap_chain_config_.device_extensions_.size(); 
    }

    const VkSwapchainKHR& GetSwapChain() const { return swap_chain_; }
    const std::vector<VkImage>& GetSwapChainImages() const { return swap_chain_images_; }
    const std::vector<VkImageView>& GetSwapChainImageViews() const { return swap_chain_image_views_; }
    const SVulkanSwapChainConfig& GetSwapChainConfig() const { return swap_chain_config_; }
    

private:
    VkSwapchainKHR swap_chain_;
    SVulkanSwapChainConfig swap_chain_config_;

    // swapchain supported properties
    std::vector<VkSurfaceFormatKHR> surface_formats_;
    std::vector<VkPresentModeKHR> present_modes_;
    VkSurfaceCapabilitiesKHR surface_capabilities_;

    // swapchain image
    std::vector<VkImage> swap_chain_images_;
    std::vector<VkImageView> swap_chain_image_views_;

    const VkDevice* device_;
    const VkPhysicalDevice* physical_device_;
    const VkSurfaceKHR* surface_;
    SDL_Window* window_;

    void CheckExtensions();
    void GetSwapChainSupport();
    bool CheckSurfaceFormat();
    bool CheckPresentMode();
    bool CheckSwapExtent();
    void CreateSwapChainInternal();
    void CreateImageViews();
};