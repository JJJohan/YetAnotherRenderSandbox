#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout(binding = 0) uniform FrameInfo 
{
    mat4 viewProj;
} frameInfo;

struct MeshInfo
{
	mat4 transform;
	vec4 color;
	uint imageIndex;
};

layout(std140, binding = 1) readonly buffer MeshInfoBuffer
{
	MeshInfo meshInfo[];
} infoBuffer;

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragUv;
layout(location = 2) flat out uint fragImageIndex;

void main() 
{
    gl_Position = frameInfo.viewProj * infoBuffer.meshInfo[gl_DrawID].transform * vec4(position, 1.0);
	fragColor = infoBuffer.meshInfo[gl_DrawID].color;
	fragUv = uv;
	fragImageIndex = infoBuffer.meshInfo[gl_DrawID].imageIndex;
}