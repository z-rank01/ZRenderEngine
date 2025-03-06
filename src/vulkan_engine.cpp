#pragma once

#include "vulkan_engine.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <chrono>
#include <thread>

static VulkanEngine* instance;

VulkanEngine* VulkanEngine::GetInstance()
{
    return instance;
}

VulkanEngine::VulkanEngine()
{
    engineState_ = EWindowState::Stopped;
}

VulkanEngine::VulkanEngine(const SEngineConfig& config)
{
    engineConfig_ = config;
    engineState_ = EWindowState::Stopped;
}

VulkanEngine::~VulkanEngine()
{
    Shutdown();
}

// Initialize the engine
void VulkanEngine::Initialize()
{
    // only one engine initialization is allowed with the application.
    assert(instance == nullptr);
    instance = this;

    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    auto window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

    window = SDL_CreateWindow(
        "Vulkan Engine", engineConfig_.window.width, engineConfig_.window.height, window_flags);

    engineState_ = EWindowState::Initialized;
}

// Main loop
void VulkanEngine::Run()
{
    engineState_ = EWindowState::Running;

    SDL_Event event;

    // main loop
    while (engineState_ != EWindowState::Stopped)
    {
        // Handle events on queue
        while (static_cast<int>(SDL_PollEvent(&event)) != 0)
        {
            // close the window when user alt-f4s or clicks the X button
            if (event.type == SDL_EVENT_QUIT)
            {
                engineState_ = EWindowState::Stopped;
            }

            if (event.window.type == SDL_EVENT_WINDOW_SHOWN)
            {
                if (event.window.type == SDL_EVENT_WINDOW_MINIMIZED)
                {
                    engineState_ = EWindowState::Stopped;
                }
                if (event.window.type == SDL_EVENT_WINDOW_RESTORED)
                {
                    engineState_ = EWindowState::Running;
                }
            }
        }

        // do not draw if we are minimized
        if (renderState_ == ERenderState::False)
        {
            // throttle the speed to avoid the endless spinning 
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        Draw();
    }
}

// Main render loop
void VulkanEngine::Draw()
{
    
}

// Shutdown the engine
void VulkanEngine::Shutdown()
{
    SDL_DestroyWindow(window);
    SDL_Quit();

    instance = nullptr;
}
