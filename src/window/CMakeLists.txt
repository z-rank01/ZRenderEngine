add_library(vulkan_window STATIC
    
)

# 设置编译器标准和选项
target_compile_features(vulkan_window PRIVATE cxx_std_20)

# 设置头文件包含目录
target_include_directories(vulkan_window
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..  # 引用其他模块目录下的头文件
)

# 链接 Vulkan 和 VulkanMemoryAllocator
target_link_libraries(vulkan_window
    PUBLIC
        SDL3::SDL3
)