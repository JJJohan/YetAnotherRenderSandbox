#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout(binding = 0) uniform FrameInfo
{
    mat4 viewProj;
    mat4 lightViewProj;
	vec4 viewPos;
	vec3 sunLightDir;
	uint debugMode;
	vec3 sunLightColor;
	float sunLightIntensity;
} frameInfo;

struct MeshInfo
{
	mat4 transform;
	mat4 normalMatrix;
	vec4 color;
	uint diffuseImageIndex;
	uint normalImageIndex;
	uint metallicRoughnessImageIndex;
};

layout(std140, binding = 1) readonly buffer MeshInfoBuffer
{
	MeshInfo meshInfo[];
} infoBuffer;

layout(location = 0) in vec3 position;

void main()
{
    gl_Position = frameInfo.lightViewProj * infoBuffer.meshInfo[gl_DrawID].transform * vec4(position, 1.0);
}