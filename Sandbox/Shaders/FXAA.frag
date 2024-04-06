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

layout(location = 0) out vec4 outColor;

#define FXAA_PC 1
#define FXAA_GLSL_130 1
#define FXAA_QUALITY_PRESET 12
#define FXAA_GREEN_AS_LUMA 1
#include "FXAA.glsl"

void main()
{
	vec2 fxaaQualityRcpFrame = vec2(1.0) / frameInfo.viewSize;

	outColor = FxaaPixelShader(
		fragUv,
		vec4(0.0),
		inputImage,
		inputImage,
		inputImage,
		fxaaQualityRcpFrame,
		vec4(0.0),
		vec4(0.0),
		vec4(0.0),
		0.75,
		0.125,
		0.0,
		0.0,
		0.0,
		0.0,
		vec4(0)
	);
}