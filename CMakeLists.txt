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
find_package(imgui CONFIG REQUIRED)
find_package(fastgltf CONFIG REQUIRED)
find_package(Stb REQUIRED)


# 添加子目录
add_subdirectory(src/utility)
add_subdirectory(src/initialization)
add_subdirectory(src/pipeline)
add_subdirectory(src/source)
add_subdirectory(src/synchronization)
add_subdirectory(src/builder)
add_subdirectory(src/vulkan_resource_allocator)
# 设置源文件
set(SOURCES
    src/main.cpp
    src/vulkan_engine.cpp
)

# 设置头文件
set(HEADERS
    src/vulkan_engine.h
)

# 添加可执行文件
add_executable(VulkanEngine ${SOURCES} ${HEADERS})

# 链接库
target_link_libraries(VulkanEngine
  PRIVATE
    Vulkan::Vulkan
    GPUOpen::VulkanMemoryAllocator
    SDL3::SDL3
    glm::glm
    vk-bootstrap::vk-bootstrap
    imgui::imgui
    fastgltf::fastgltf
    utility                         # 添加 utility 库
    vulkan_initialization           # 添加 vulkan_initialization 库
    vulkan_pipeline                 # 添加 vulkan_pipeline 库
    vulkan_source                   # 添加 vulkan_source 库
    vulkan_synchronization          # 添加 vulkan_synchronization 库
    builder                         # 添加 builder 库
    vulkan_resource_allocator       # 添加 vra 库
)

# 包含目录
target_include_directories(VulkanEngine
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/build/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/include
    ${Vulkan_INCLUDE_DIRS}
)

# 设置特定于编译器的警告选项
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Clang 可能的 ID 包括 "Clang", "AppleClang" 等
    target_compile_options(VulkanEngine PRIVATE -Wall)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(VulkanEngine PRIVATE /W4)
endif()

# 使用生成器表达式为 Debug 构建添加调试信息选项
target_compile_options(VulkanEngine PRIVATE
    $<$<CONFIG:Debug>:$<$<CXX_COMPILER_ID:GNU,Clang>:-g>>
    $<$<CONFIG:Debug>:$<$<CXX_COMPILER_ID:MSVC>:/Zi>>
    $<$<CONFIG:Release>:$<$<CXX_COMPILER_ID:GNU,Clang>:-O3>>
    $<$<CONFIG:Release>:$<$<CXX_COMPILER_ID:MSVC>:/O2>>
)

# 设置 IDE 中的源文件分组
source_group(
    TREE ${CMAKE_CURRENT_SOURCE_DIR}
    PREFIX ""
    FILES ${SOURCES} ${HEADERS}
)

# 查询 Vulkan SDK 路径
if (Vulkan_FOUND)
    message(STATUS "Vulkan SDK found at: ${Vulkan_INCLUDE_DIRS}")
    message(STATUS "Vulkan Libraries: ${Vulkan_LIBRARIES}")
    message(STATUS "Vulkan Version: ${Vulkan_VERSION}")
else()
    message(FATAL_ERROR "Vulkan SDK not found. Please install the Vulkan SDK.")
endif()