add_library(gltf_helper STATIC
    gltf_loader.h
    gltf_data.h
    gltf_parser.h
    gltf_parser.cpp
    gltf_converter.h
    tiny_gltf_impl.cpp
)

# 设置编译器标准和选项
target_compile_features(gltf_helper PRIVATE cxx_std_20)

# 设置 tiny_gltf 的定义
# target_compile_definitions(gltf_helper 
#     PRIVATE 
#         TINYGLTF_IMPLEMENTATION
#         STB_IMAGE_IMPLEMENTATION
#         TINYGLTF_NO_STB_IMAGE_WRITE
# )

# 设置头文件包含目录
target_include_directories(gltf_helper
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..  # 允许引用其他模块目录下的头文件
        ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-mingw-static/include  # tiny_gltf 头文件路径
)

# 链接 Vulkan 和其他依赖
target_link_libraries(gltf_helper
    PUBLIC
        Vulkan::Vulkan
)