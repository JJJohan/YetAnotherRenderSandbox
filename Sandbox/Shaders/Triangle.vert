#version 460

layout(binding = 0) uniform UniformBufferObject 
{
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 modelViewProj;
} ubo;

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 colour;

layout(location = 0) out vec4 fragColor;

void main() 
{
    gl_Position = ubo.modelViewProj * vec4(position, 1.0);
    fragColor = colour;
}