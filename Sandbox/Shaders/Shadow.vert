#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_shader_viewport_layer_array : require

#include "MeshInfo.glsl"

layout(binding = 0) uniform FrameInfo
{
    mat4 viewProj;
    mat4 prevViewProj;
    mat4 view;
	vec4 viewPos;
	vec2 viewSize;
	vec2 jitter;
} frameInfo;

layout(binding = 1) uniform LightData
{
	vec4 cascadeSplits;
    mat4 cascadeMatrices[4];
	vec3 sunLightColor;
	float sunLightIntensity;
	vec3 sunLightDir;
} lightData;

layout(push_constant) uniform PushConsts {
	uint cascadeIndex;
} pushConsts;

layout(std140, binding = 2) readonly buffer MeshInfoBuffer
{
	MeshInfo meshInfo[];
} infoBuffer;

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;

layout(location = 0) flat out uint fragDiffuseImageIndex;

layout(location = 1) out vec4 fragColor;
layout(location = 2) out vec2 fragUv;

void main()
{
    gl_Position = lightData.cascadeMatrices[pushConsts.cascadeIndex] * infoBuffer.meshInfo[gl_InstanceIndex].transform * vec4(position, 1.0);

	fragDiffuseImageIndex = infoBuffer.meshInfo[gl_InstanceIndex].diffuseImageIndex;
	fragColor = infoBuffer.meshInfo[gl_InstanceIndex].color;
	fragUv = uv;

	gl_Layer = int(pushConsts.cascadeIndex);
}