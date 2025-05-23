add_library(vulkan_synchronization STATIC
    vulkan_synchronization.cpp
    vulkan_synchronization.h
)

# 设置编译器标准和选项
target_compile_features(vulkan_synchronization PRIVATE cxx_std_20)

# 设置头文件包含目录
target_include_directories(vulkan_synchronization
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..  # 引用其他模块目录下的头文件
)

# 链接 Vulkan 和 VulkanMemoryAllocator
target_link_libraries(vulkan_synchronization
    PUBLIC
        Vulkan::Vulkan
)