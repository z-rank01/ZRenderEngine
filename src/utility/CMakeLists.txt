# 添加库：vulkan_logger
add_library(utility STATIC
    logger.h
    logger.cpp
)

# 设置头文件包含目录
target_include_directories(utility
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..  # 允许其他模块引用 utility 目录下的头文件 
)

# 设置编译器标准和选项
target_compile_features(utility PRIVATE cxx_std_20)

# 链接 Vulkan
target_link_libraries(utility
    PUBLIC
        Vulkan::Vulkan
)
