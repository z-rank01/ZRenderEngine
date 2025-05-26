add_library(template STATIC
    common.h
    common.cpp
)

# 设置编译器标准和选项
target_compile_features(template PRIVATE cxx_std_20)

# 设置头文件包含目录
target_include_directories(template
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..  # 引用其他模块目录下的头文件
)

# 链接库
target_link_libraries(template
    PUBLIC
        Vulkan::Vulkan
)