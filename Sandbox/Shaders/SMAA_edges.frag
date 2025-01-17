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

layout(binding = 1) uniform sampler2D inputImage;

layout(location = 0) in vec2 fragUv;
layout(location = 1) in vec4 offsets[3];

layout(location = 0) out vec2 outColor;

#define SMAA_INCLUDE_VS 0
#define SMAA_RT_METRICS vec4(vec2(1.0) / frameInfo.viewSize, frameInfo.viewSize)
#define SMAA_GLSL_4
#define SMAA_PRESET_HIGH
#include "SMAA.glsl"

void main()
{
	outColor = SMAAColorEdgeDetectionPS(fragUv, offsets, inputImage);
}