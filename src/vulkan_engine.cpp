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
    engineState_ = EEngineState::Stopped;
}

VulkanEngine::VulkanEngine(const SEngineConfig& config)
{
    engineConfig_ = config;
    engineState_ = EEngineState::Stopped;
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

    window_ = SDL_CreateWindow(
        "Vulkan Engine", engineConfig_.window.width, engineConfig_.window.height, window_flags);

    engineState_ = EEngineState::Initialized;
}

// Main loop
void VulkanEngine::Run()
{
    engineState_ = EEngineState::Running;

    SDL_Event event;

    // main loop
    while (engineState_ == EEngineState::Running)
    {
        // Handle events on queue
        while (static_cast<int>(SDL_PollEvent(&event)) != 0)
        {
            // close the window when user alt-f4s or clicks the X button
            if (event.type == SDL_EVENT_QUIT)
            {
                engineState_ = EEngineState::Stopped;
            }

            if (event.window.type == SDL_EVENT_WINDOW_SHOWN)
            {
                if (event.window.type == SDL_EVENT_WINDOW_MINIMIZED)
                {
                    engineState_ = EEngineState::Stopped;
                }
                if (event.window.type == SDL_EVENT_WINDOW_RESTORED)
                {
                    engineState_ = EEngineState::Running;
                }
            }
        }

        // do not draw if we are minimized
        if (engineState_ == EEngineState::Stopped)
        {
            // throttle the speed to avoid the endless spinning
            const auto sleep_time = std::chrono::milliseconds(100);
            std::this_thread::sleep_for(sleep_time);
            continue;
        }

        Draw();
    }
}

// Main render loop
void VulkanEngine::Draw()
{
    engineState_ = EEngineState::Drawing;
}

// Shutdown the engine
void VulkanEngine::Shutdown()
{
    if (engineState_ == EEngineState::Stopped)
    {
        return;
    }

    engineState_ = EEngineState::Stopped;

    SDL_DestroyWindow(window_);
    SDL_Quit();

    instance = nullptr;
}
