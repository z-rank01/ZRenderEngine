add_library(vulkan_pipeline STATIC
    vulkan_shader.cpp
    vulkan_shader.h
    vulkan_pipeline.cpp
    vulkan_pipeline.h
    vulkan_renderpass.cpp
    vulkan_renderpass.h
)

# 设置编译器标准和选项
target_compile_features(vulkan_pipeline PRIVATE cxx_std_20)

# 设置头文件包含目录
target_include_directories(vulkan_pipeline
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..  # 引用其他模块目录下的头文件
)

# 链接 Vulkan 和 VulkanMemoryAllocator
target_link_libraries(vulkan_pipeline
    PUBLIC
        Vulkan::Vulkan
)