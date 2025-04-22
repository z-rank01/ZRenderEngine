add_library(vulkan_source STATIC
    vulkan_framebuffer.cpp
    vulkan_framebuffer.h
    vulkan_commandbuffer.cpp
    vulkan_commandbuffer.h
)

# 设置编译器标准和选项
target_compile_features(vulkan_source PRIVATE cxx_std_20)

# 设置头文件包含目录
target_include_directories(vulkan_source
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..  # 允许 initialization 引用其他模块目录下的头文件
)

# 链接 Vulkan 和 VulkanMemoryAllocator
target_link_libraries(vulkan_source
    PUBLIC
        Vulkan::Vulkan
)