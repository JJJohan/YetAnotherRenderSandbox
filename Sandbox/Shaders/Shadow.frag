#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout(binding = 3) uniform sampler samp;
layout(binding = 4) uniform texture2D textures[];

layout(location = 0) flat in uint fragDiffuseImageIndex;

layout(location = 1) in vec4 fragColor;
layout(location = 2) in vec2 fragUv;

void main()
{
	float alpha = texture(sampler2D(textures[fragDiffuseImageIndex], samp), fragUv).a * fragColor.a;
	if (alpha < 0.5) discard;
}