#include <iostream>
#include <cstdlib>
#include <vulkan/vulkan.h>
#include <VkBootstrap.h>
#include <glm/glm.hpp>
#include <fmt/core.h>
#include <imgui.h>
#include <fastgltf/core.hpp>
#include <stb_image.h>

int main() {
    std::cout << "Testing C++ Development Environment Configuration" << '\n';

    std::cout << "Hello, World!" << '\n';

    std::cout << "\nChecking CMake version:" << '\n';
    std::system("cmake --version");

    std::cout << "\nPackages has been installed." << '\n';

    return 0;
}