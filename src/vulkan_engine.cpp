#pragma once

#include "vulkan_engine.h"
// #include <SDL3/SDL.h>
// #include <SDL3/SDL_vulkan.h>
#include <chrono>
#include <thread>

VulkanEngine* instance = nullptr;

VulkanEngine& VulkanEngine::GetInstance()
{
    return *instance;
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
    // auto window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);    // failed when there is no vulkan driver
    auto window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE);
    window_ = SDL_CreateWindow("Vulkan Engine", engineConfig_.window.width, engineConfig_.window.height, window_flags);
    renderer_ = SDL_CreateRenderer(window_, "Vulkan Renderer");

    // show window (has implicitly created after SDL_CreateWindow)
    SDL_ShowWindow(window_);

    // Initialize the states
    engineState_ = EWindowState::Initialized;
    renderState_ = ERenderState::True;
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
        while (SDL_PollEvent(&event))
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
                    renderState_ = ERenderState::False;
                }
                if (event.window.type == SDL_EVENT_WINDOW_RESTORED)
                {
                    renderState_ = ERenderState::True;
                }
            }
        }

        // do not draw if we are minimized
        if (renderState_ == ERenderState::False)
        {
            // throttle the speed to avoid the endless spinning 
            constexpr auto sleep_duration_ms = 100;
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_duration_ms));
            continue;
        }

        Draw();
    }
}

// Main render loop
void VulkanEngine::Draw()
{
    SDL_RenderClear(renderer_);
    // TODO: Add your rendering code here
    SDL_RenderTexture(renderer_, nullptr, 0, 0);
    SDL_RenderPresent(renderer_);  // 显示渲染的内容
}

// Shutdown the engine
void VulkanEngine::Shutdown()
{
    SDL_DestroyRenderer(renderer_);
    SDL_DestroyWindow(window_);
    SDL_Quit();

    instance = nullptr;
}
