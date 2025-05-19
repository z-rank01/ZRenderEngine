add_library(gltf_helper STATIC
    gltf_loader.h
    gltf_data.h
    gltf_parser.h
)

# 设置编译器标准和选项
target_compile_features(gltf_helper PRIVATE cxx_std_20)

# 设置头文件包含目录
target_include_directories(gltf_helper
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..  # 允许 initialization 引用其他模块目录下的头文件
)

# 链接 Vulkan 和 VulkanMemoryAllocator
target_link_libraries(gltf_helper
    PUBLIC
        Vulkan::Vulkan
        fastgltf::fastgltf
)