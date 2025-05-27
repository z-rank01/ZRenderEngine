add_library(vulkan_resource_allocator STATIC
    vra.cpp
    vra.h
    pool.cpp
    pool.h
)

# 设置头文件包含目录
target_include_directories(vulkan_resource_allocator
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..  # 引用其他模块目录下的头文件
)

# 链接 Vulkan 和 VulkanMemoryAllocator
target_link_libraries(vulkan_resource_allocator
    PUBLIC
        Vulkan::Vulkan
        GPUOpen::VulkanMemoryAllocator
)