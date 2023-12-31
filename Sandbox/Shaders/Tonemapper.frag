#version 460
#extension GL_EXT_samplerless_texture_functions : require

layout(binding = 0) uniform sampler nearestSampler;
layout(binding = 1) uniform texture2D inputImage;

layout(location = 0) in vec2 fragUv;

layout(location = 0) out vec4 outColor;

void main()
{
	vec3 inputColor = texture(sampler2D(inputImage, nearestSampler), fragUv).rgb;
	outColor = vec4(inputColor, 1.0);
}