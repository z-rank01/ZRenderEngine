add_library(rendergraph STATIC
    rendergraph.h
    rendergraph.cpp
)

# 设置头文件包含目录
target_include_directories(rendergraph
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..  # 允许引用其他模块目录下的头文件
        ${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/include  # tiny_gltf 头文件路径
)

# 链接 Vulkan 和其他依赖
target_link_libraries(rendergraph
    PUBLIC
        Vulkan::Vulkan
        glm::glm
)