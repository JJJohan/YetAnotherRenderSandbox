#version 460
#extension GL_EXT_samplerless_texture_functions : require

layout(binding = 0) uniform sampler nearestSampler;
layout(binding = 1) uniform texture2D inputImage;

layout(location = 0) in vec2 fragUv;

layout(location = 0) out vec4 outColor;

layout (constant_id = 0) const uint isHdr = 0U;

void main()
{
	const float gamma = 2.2;
	vec3 inputColor = texture(sampler2D(inputImage, nearestSampler), fragUv).rgb;

	if (isHdr != 0)
	{
		inputColor = pow(inputColor, vec3(1.0 / gamma));
		outColor = vec4(inputColor, 1.0);
	}
	else
	{
		// Simple reinhard
		inputColor = inputColor / (1.0f + inputColor);
		outColor = vec4(inputColor, 1.0);
	}
}