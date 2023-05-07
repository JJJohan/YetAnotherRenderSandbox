#version 460

layout(binding = 0) uniform UniformBufferObject 
{
    mat4 viewProj;
} frameInfo;

layout(push_constant) uniform constants
{
	mat4 transform;
	vec4 color;
} pc;

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragUv;

void main() 
{
    gl_Position = frameInfo.viewProj * pc.transform * vec4(position, 1.0);
	fragColor = pc.color;
	fragUv = uv;
}