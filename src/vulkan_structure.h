#pragma once

// Includes standard library
#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

// Includes Vulkan
#include <vk_mem_alloc.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

// Includes fmt
#include <fmt/core.h>

// Includes GLM
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

// Macro to check Vulkan results
#define VK_CHECK(x)                                                          \
    do                                                                       \
    {                                                                        \
        VkResult err = x;                                                    \
        if (err)                                                             \
        {                                                                    \
            fmt::println("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                         \
        }                                                                    \
    } while (0)