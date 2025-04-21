add_library(vulkan_initialization STATIC
    vulkan_instance.cpp
    vulkan_instance.h
    vulkan_device.cpp
    vulkan_device.h
    vulkan_queue.h
    vulkan_queue.cpp
    vulkan_window.h
    vulkan_window.cpp
)

# 设置头文件包含目录
target_include_directories(vulkan_initialization
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..  # 允许 initialization 引用其他目录下的头文件
)

# 设置编译器标准和选项
target_compile_features(vulkan_initialization PRIVATE cxx_std_20)

# 链接 Vulkan 和 VulkanMemoryAllocator
target_link_libraries(vulkan_initialization
    PUBLIC
        Vulkan::Vulkan
)