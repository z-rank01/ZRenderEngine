#ifndef GLTF_DATA_H
#define GLTF_DATA_H

#include "gltf/gltf_loader.h"
#include <fastgltf/core.hpp>
#include <glm/glm.hpp>

namespace gltf
{
    /// @brief texture format.
    /// @note 0: uint8, 1: uint16, 2: uint32, 3: float16, 4: float32.
    enum TextureFormat
    {
        UINT8,
        UINT16,
        UINT32,
        FLOAT16,
        FLOAT32
    };

    /// @brief texture filter.
    enum TextureFilter
    {
        NEAREST,
        LINEAR,
        NEAREST_MIPMAP_NEAREST,
        LINEAR_MIPMAP_NEAREST,
        NEAREST_MIPMAP_LINEAR,
        LINEAR_MIPMAP_LINEAR
    };

    /// @brief texture wrap.
    enum TextureWrap
    {
        REPEAT,
        MIRRORED_REPEAT,
        CLAMP_TO_EDGE,
        CLAMP_TO_BORDER
    };

    /// @brief texture sampler.
    struct TextureSampler
    {
        TextureFilter min_filter;
        TextureFilter mag_filter;
        TextureWrap s_wrap;
        TextureWrap t_wrap;
    };

    /// @brief texture.
    /// @note data is stored as uint8_t.
    struct Texture
    {
        void *data;
        uint64_t width;
        uint64_t height;
        uint64_t channels;
        TextureFormat format;
        TextureSampler sampler;
    };

    /// @brief inputs of a primitive defined in gltf file.
    /// @note tangent is stored as vec4 to avoid padding.
    struct VertexInput
    {
        glm::vec3 position;
        glm::vec4 color;
        glm::vec3 normal;
        glm::vec4 tangent;
        glm::vec2 uv0;
        glm::vec2 uv1;
    };

    /// @brief base pbr material.
    /// @note metallic roughness workflow.
    struct BasePbrMaterial
    {
        // pbr factors
        glm::vec4 base_color_factor;
        float metallic_factor;
        float roughness_factor;

        // texture factors
        float normal_texture_scale;
        float occlusion_strength;
        glm::vec3 emissive_factor;
        
        // alpha
        float alpha_cutoff;
        uint8_t alpha_mode; // 0: Opaque, 1: Mask, 2: Blend
        bool double_sided;

        // texture texcoord set
        bool base_color_texture_texcoord_set;
        bool metallic_roughness_texture_texcoord_set;
        bool normal_texture_texcoord_set;
        bool occlusion_texture_texcoord_set;
        bool emissive_texture_texcoord_set;

        // texture
        Texture base_color_texture;
        Texture metallic_roughness_texture;
        Texture normal_texture;
        Texture occlusion_texture;
        Texture emissive_texture;
    };

    /// @brief single draw call data.
    /// @note equals to a primitives element in gltf file.
    struct PerDrawCallData
    {
        glm::mat4 transform;
        std::vector<uint32_t> indices;
        std::vector<VertexInput> vertex_inputs;
        uint32_t material_index;
    };

    /// @brief mesh data containing multiple primitives.
    struct PerMeshData
    {
        std::string name;
        std::vector<PerDrawCallData> primitives;
    };
}

#endif // GLTF_DATA_H
