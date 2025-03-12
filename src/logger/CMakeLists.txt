# 添加库：vulkan_logger
add_library(vulkan_logger STATIC
    vulkan_logger.h
    vulkan_logger.cpp
)

# 设置头文件包含目录
target_include_directories(vulkan_logger
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..  # 允许其他模块引用 logger 目录下的头文件 
)

# 设置编译器标准和选项
target_compile_features(vulkan_logger PRIVATE cxx_std_20)

# 链接 Vulkan
target_link_libraries(vulkan_logger
    PUBLIC
        Vulkan::Vulkan
)
