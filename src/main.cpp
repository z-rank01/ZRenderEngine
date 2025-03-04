#include "vulkan_engine.h"

auto main() -> int
{
    // window config
    SWindowConfig window_config;
    const auto window_width = 800;
    const auto window_height = 600;
    const auto* window_name = "Vulkan Engine";

    window_config.width = window_width;
    window_config.height = window_height;
    window_config.title = window_name;

    // engine config
    SEngineConfig config;
    config.window = window_config;
    config.frame_count = 2;
    config.use_validation_layers = true;

    // main loop
    VulkanEngine engine(config);
    engine.Initialize();
    engine.Run();
    engine.Shutdown();

    return 0;
}