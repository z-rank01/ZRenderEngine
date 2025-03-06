#pragma once

#include "vulkan_structure.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

enum class EWindowState : std::uint8_t
{
    Initialized, // Engine is initialized but not running
    Running,     // Engine is running
    Stopped      // Engine is stopped
};

enum class ERenderState : std::uint8_t
{
    True,   // Render is enabled 
    False   // Render is disabled
};

struct SWindowConfig
{
    int width;
    int height;
    std::string title;

    [[nodiscard]] constexpr auto Validate() const -> bool
    {
        return width > 0 && height > 0;
    }
};

struct SEngineConfig
{
    SWindowConfig window;
    uint8_t frame_count;
    bool use_validation_layers;
};

class VulkanEngine
{
public:
    VulkanEngine();
    VulkanEngine(const SEngineConfig& config);
    ~VulkanEngine();

    void Initialize();
    void Run();
    void Draw();
    void Shutdown();

    static VulkanEngine& GetInstance();

private:
    EWindowState engineState_;
    ERenderState renderState_;
    SEngineConfig engineConfig_;
    SDL_Window* window_;
    SDL_Renderer* renderer_;
};
