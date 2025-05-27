add_library(gltf_helper STATIC
    gltf_loader.h
    gltf_data.h
    gltf_parser.h
    gltf_parser.cpp
    gltf_converter.h
    tiny_gltf_impl.cpp
    test.h
    test.cpp
)

# 设置头文件包含目录
target_include_directories(gltf_helper
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..  # 允许引用其他模块目录下的头文件
        ${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/include  # tiny_gltf 头文件路径
)

# 链接 Vulkan 和其他依赖
target_link_libraries(gltf_helper
    PUBLIC
        Vulkan::Vulkan
        glm::glm  # 添加 glm 依赖，gltf 通常需要数学库
)