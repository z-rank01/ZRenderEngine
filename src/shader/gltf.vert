#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec4 inTangent;
layout(location = 4) in vec2 inTexCoord0;
layout(location = 5) in vec2 inTexCoord1;

layout(location = 0) out vec3 fragColor;

layout(binding = 0) uniform MvpMatrix {
    mat4 model;
    mat4 view;
    mat4 proj;
} mvp_matrix;

void main() 
{
    gl_Position = mvp_matrix.proj * mvp_matrix.view * mvp_matrix.model * vec4(inPosition, 1.0);
    fragColor = (mvp_matrix.model * vec4(inPosition, 1.0)).xyz;
}