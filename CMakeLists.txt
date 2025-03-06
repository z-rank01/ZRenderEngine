cmake_minimum_required(VERSION 3.20)
project(VulkanEngine LANGUAGES CXX)

# 启用 C++20 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置 vcpkg 工具链文件（如果未通过命令行指定）
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "Vcpkg toolchain file")
endif()

# 设置 vcpkg 清单模式
set(VCPKG_MANIFEST_MODE ON)

# 查找依赖
find_package(Vulkan REQUIRED)
find_package(vk-bootstrap CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(fastgltf CONFIG REQUIRED)
find_package(Stb REQUIRED)

# 设置源文件
set(SOURCES
    src/main.cpp
    src/vulkan_engine.cpp
)

# 设置头文件
set(HEADERS
    src/vulkan_engine.h
    src/vulkan_structure.h
)

# 添加可执行文件
add_executable(VulkanEngine ${SOURCES} ${HEADERS})

# 链接库
target_link_libraries(VulkanEngine
  PRIVATE
    Vulkan::Vulkan
    vk-bootstrap::vk-bootstrap
    GPUOpen::VulkanMemoryAllocator
    SDL3::SDL3
    glm::glm
    fmt::fmt-header-only
    imgui::imgui
    fastgltf::fastgltf
)

# 包含目录
target_include_directories(VulkanEngine
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/${VCPKG_TARGET_TRIPLET}/include # STB 头文件路径
)

# 定义宏以使用 Vulkan 动态加载
target_compile_definitions(VulkanEngine PRIVATE VK_NO_PROTOTYPES)

# 设置 IDE 中的源文件分组
source_group(
    TREE ${CMAKE_CURRENT_SOURCE_DIR}
    PREFIX ""
    FILES ${SOURCES} ${HEADERS}
)