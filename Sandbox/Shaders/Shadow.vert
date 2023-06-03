#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout(binding = 0) uniform FrameInfo
{
    mat4 viewProj;
    mat4 prevViewProj;
    mat4 view;
	vec4 viewPos;
	vec2 viewSize;
	vec2 jitter;
	uint debugMode;
} frameInfo;

layout(binding = 1) uniform LightData
{
	vec4 cascadeSplits;
    mat4 cascadeMatrices[4];
	vec3 sunLightColor;
	float sunLightIntensity;
	vec3 sunLightDir;
	float padding;
} lightData;

struct MeshInfo
{
	mat4 transform;
	mat4 normalMatrix;
	vec4 color;
	uint diffuseImageIndex;
	uint normalImageIndex;
	uint metallicRoughnessImageIndex;
};

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
    gl_Position = lightData.cascadeMatrices[pushConsts.cascadeIndex] * infoBuffer.meshInfo[gl_DrawID].transform * vec4(position, 1.0);

	fragDiffuseImageIndex = infoBuffer.meshInfo[gl_DrawID].diffuseImageIndex;
	fragColor = infoBuffer.meshInfo[gl_DrawID].color;
	fragUv = uv;
}