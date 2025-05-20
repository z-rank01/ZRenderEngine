#ifndef GLTF_CONVERTER_H
#define GLTF_CONVERTER_H

#include <vulkan/vulkan.h>
#include <numeric>
#include <vector>

#include "gltf_data.h"

namespace gltf
{
    // --- helper function objects ---

    /// Convert per mesh data to list of indices
    class Mesh2Indices
    {
    public:
        Mesh2Indices() = default;
        ~Mesh2Indices() = default;

        std::vector<uint32_t> operator()(const std::vector<PerMeshData> &meshes)
        {
            std::vector<uint32_t> indices;

            // reserve space for indices(per mesh and per primitive)
            const auto indices_count = std::accumulate(
                meshes.begin(),
                meshes.end(),
                0,
                [](int sum, const PerMeshData &mesh)
                {
                    return std::accumulate(
                        mesh.primitives.begin(),
                        mesh.primitives.end(),
                        sum,
                        [](int sum, const PerDrawCallData &primitive)
                        { return sum + primitive.indices.size(); });
                });
            indices.reserve(indices_count);

            // convert primitives to indices
            std::for_each(
                meshes.begin(), 
                meshes.end(), 
                [&indices](const PerMeshData& mesh) 
                {
                    std::for_each(
                        mesh.primitives.begin(), 
                        mesh.primitives.end(),
                        [&indices](const PerDrawCallData& primitive) 
                        {
                            std::copy(primitive.indices.begin(), 
                                    primitive.indices.end(), 
                                    std::back_inserter(indices));
                        });
                });
            return indices;
        }
    };


    /// Convert per mesh data to list of vertices
    class Mesh2Vertices
    {
    public:
        Mesh2Vertices() = default;
        ~Mesh2Vertices() = default;

        std::vector<VertexInput> operator()(const std::vector<PerMeshData> &meshes)
        {
            std::vector<VertexInput> vertices;
            
            // reserve space for vertices(per mesh and per primitive)
            const auto vertices_count = std::accumulate(
                meshes.begin(),
                meshes.end(),
                0,
                [](int sum, const PerMeshData &mesh)
                {
                    return std::accumulate(
                        mesh.primitives.begin(),
                        mesh.primitives.end(),
                        sum,
                        [](int sum, const PerDrawCallData &primitive)
                        { return sum + primitive.vertex_inputs.size(); });
                });
            vertices.reserve(vertices_count);

            // convert primitives to vertices
            std::for_each(
                meshes.begin(),
                meshes.end(),
                [&vertices](const PerMeshData &mesh)
                {
                    std::for_each(
                        mesh.primitives.begin(),
                        mesh.primitives.end(),
                        [&vertices](const PerDrawCallData &primitive)
                        {
                            std::copy(primitive.vertex_inputs.begin(),
                                    primitive.vertex_inputs.end(),
                                    std::back_inserter(vertices));
                        });
                });
            return vertices;
        }
    };

    /// Convert per draw call data to list of indices
    class DrawCalls2Indices
    {
    public:
        DrawCalls2Indices() = default;
        ~DrawCalls2Indices() = default;

        std::vector<uint32_t> operator()(const std::vector<PerDrawCallData> &draw_calls)
        {
            std::vector<uint32_t> indices;

            // reserve space for indices(per draw call)
            const auto indices_count = std::accumulate(
                draw_calls.begin(),
                draw_calls.end(),
                0,
                [](int sum, const PerDrawCallData &draw_call)
                { return sum + draw_call.indices.size(); });
            indices.reserve(indices_count);

            // convert draw calls to indices
            std::for_each(
                draw_calls.begin(),
                draw_calls.end(),
                [&indices](const PerDrawCallData &draw_call)
                {
                    std::copy(draw_call.indices.begin(), draw_call.indices.end(), std::back_inserter(indices));
                });
            return indices;
        }
    };

    /// Convert per draw call data to list of vertices
    class DrawCalls2Vertices
    {
    public:
        DrawCalls2Vertices() = default;
        ~DrawCalls2Vertices() = default;

        std::vector<VertexInput> operator()(const std::vector<PerDrawCallData> &draw_calls)
        {
            std::vector<VertexInput> vertices;

            // reserve space for vertices(per draw call)
            const auto vertices_count = std::accumulate(
                draw_calls.begin(),
                draw_calls.end(),
                0,
                [](int sum, const PerDrawCallData &draw_call)
                { return sum + draw_call.vertex_inputs.size(); });
            vertices.reserve(vertices_count);

            // convert draw calls to vertices
            std::for_each(
                draw_calls.begin(),
                draw_calls.end(),
                [&vertices](const PerDrawCallData &draw_call)
                {
                    std::copy(draw_call.vertex_inputs.begin(), draw_call.vertex_inputs.end(), std::back_inserter(vertices));
                });
            return vertices;
        }
    };
}

#endif // GLTF_CONVERTER_H
