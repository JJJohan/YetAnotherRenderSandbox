#version 460

layout(binding = 0) uniform FrameInfo
{
    mat4 viewProj;
    mat4 prevViewProj;
    mat4 view;
	vec4 viewPos;
	vec2 viewSize;
	vec2 jitter;
} frameInfo;

layout(location = 0) out vec2 fragUv;
layout(location = 1) out vec4 offset;

#define SMAA_INCLUDE_PS 0
#define SMAA_RT_METRICS vec4(vec2(1.0) / frameInfo.viewSize, frameInfo.viewSize)
#define SMAA_GLSL_4
#define SMAA_PRESET_HIGH
#include "SMAA.glsl"

void main()
{
	vec2 vertices[3] = vec2[3](vec2(-1.0, 3.0), vec2(3.0, -1.0), vec2(-1.0, -1.0));
	gl_Position = vec4(vertices[gl_VertexIndex], 0.0, 1.0);
	fragUv = 0.5 * gl_Position.xy + vec2(0.5);
	
	SMAANeighborhoodBlendingVS(fragUv, offset);
}