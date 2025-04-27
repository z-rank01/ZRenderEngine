add_library(builder STATIC
    builder.h
    builder.cpp
)

# 设置头文件包含目录
target_include_directories(builder
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# 设置编译器标准和选项
target_compile_features(builder PRIVATE cxx_std_20)