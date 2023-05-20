#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout(binding = 0) uniform FrameInfo
{
    mat4 viewProj;
	vec4 viewPos;
	vec4 ambientColor;
	vec4 sunLightDir;
	vec4 sunLightColor;
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
layout(location = 1) in vec2 uv;
layout(location = 2) in vec3 normal;

layout(location = 0) flat out uint fragDiffuseImageIndex;
layout(location = 1) flat out uint fragNormalImageIndex;
layout(location = 2) flat out uint fragMetallicRoughnessImageIndex;

layout(location = 3) out vec4 fragColor;
layout(location = 4) out vec2 fragUv;
layout(location = 5) out vec3 fragWorldPos;
layout(location = 6) out vec3 fragNormal;

void main()
{
	vec4 transformedPos = infoBuffer.meshInfo[gl_DrawID].transform * vec4(position, 1.0);

	fragDiffuseImageIndex = infoBuffer.meshInfo[gl_DrawID].diffuseImageIndex;
	fragNormalImageIndex = infoBuffer.meshInfo[gl_DrawID].normalImageIndex;
	fragMetallicRoughnessImageIndex = infoBuffer.meshInfo[gl_DrawID].metallicRoughnessImageIndex;

	fragColor = infoBuffer.meshInfo[gl_DrawID].color;
	fragUv = uv;
	fragWorldPos = transformedPos.xyz;
	fragNormal = mat3(infoBuffer.meshInfo[gl_DrawID].normalMatrix) * normal;

    gl_Position = frameInfo.viewProj * transformedPos;
}