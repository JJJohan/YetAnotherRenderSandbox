#version 460
#extension GL_EXT_nonuniform_qualifier : require

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
layout(location = 5) out vec4 fragPrevPos;
layout(location = 6) out vec4 fragPos;
layout(location = 7) out vec4 fragWorldPosAndViewDepth;
layout(location = 8) out vec3 fragNormal;

void main()
{
	vec4 transformedPos = infoBuffer.meshInfo[gl_InstanceIndex].transform * vec4(position, 1.0);

	fragDiffuseImageIndex = infoBuffer.meshInfo[gl_InstanceIndex].diffuseImageIndex;
	fragNormalImageIndex = infoBuffer.meshInfo[gl_InstanceIndex].normalImageIndex;
	fragMetallicRoughnessImageIndex = infoBuffer.meshInfo[gl_InstanceIndex].metallicRoughnessImageIndex;

	fragColor = infoBuffer.meshInfo[gl_InstanceIndex].color;
	fragUv = uv;

    fragWorldPosAndViewDepth.xyz = transformedPos.xyz / transformedPos.w;
	fragWorldPosAndViewDepth.w = (frameInfo.view * transformedPos).z;
    fragNormal = normalize(vec3(infoBuffer.meshInfo[gl_InstanceIndex].normalMatrix * vec4(normal, 0.0)));
	fragPrevPos = frameInfo.prevViewProj * vec4(fragWorldPosAndViewDepth.xyz, 1.0);

	fragWorldPosAndViewDepth.xyz += frameInfo.viewPos.xyz;
    gl_Position = fragPos = frameInfo.viewProj * transformedPos;
    gl_Position.xy += frameInfo.jitter * fragPos.w; // Apply Jittering
}