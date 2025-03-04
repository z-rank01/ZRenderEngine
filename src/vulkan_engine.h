#pragma once

#include "vulkan_structure.h"

enum class EEngineState : std::uint8_t
{
    Initialized, // Engine is initialized but not running
    Running,     // Engine is running
    Drawing,     // Engine is drawing
    Stopped      // Engine is stopped
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

    static auto GetInstance() -> VulkanEngine*;

private:
    EEngineState engineState_;
    SEngineConfig engineConfig_;
    struct SDL_Window* window_;
};
