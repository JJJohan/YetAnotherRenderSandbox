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
layout(location = 3) in vec3 tangent;
layout(location = 4) in vec3 bitangent;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragUv;
layout(location = 2) flat out uint fragDiffuseImageIndex;
layout(location = 3) flat out uint fragNormalImageIndex;
layout(location = 4) flat out uint fragMetallicRoughnessImageIndex;

layout(location = 5) out vec3 fragTangentLightPos;
layout(location = 6) out vec3 fragTangentViewPos;
layout(location = 7) out vec3 fragTangentFragPos;

void main()
{
	vec4 transformedPos = infoBuffer.meshInfo[gl_DrawID].transform * vec4(position, 1.0);

	fragColor = infoBuffer.meshInfo[gl_DrawID].color;
	fragUv = uv;

	mat3 normalMatrix = transpose(inverse(mat3(infoBuffer.meshInfo[gl_DrawID].transform)));
    vec3 T = normalize(normalMatrix * tangent);
    vec3 N = normalize(normalMatrix * normal);
    vec3 B = normalize(normalMatrix * bitangent);

    mat3 TBN = transpose(mat3(T, B, N));
    fragTangentLightPos = TBN * (transformedPos.xyz - frameInfo.sunLightDir.xyz);
    fragTangentViewPos = TBN * frameInfo.viewPos.xyz;
    fragTangentFragPos = TBN * transformedPos.xyz;

	fragDiffuseImageIndex = infoBuffer.meshInfo[gl_DrawID].diffuseImageIndex;
	fragNormalImageIndex = infoBuffer.meshInfo[gl_DrawID].normalImageIndex;
	fragMetallicRoughnessImageIndex = infoBuffer.meshInfo[gl_DrawID].metallicRoughnessImageIndex;

    gl_Position = frameInfo.viewProj * transformedPos;
}