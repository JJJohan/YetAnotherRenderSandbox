#version 460
#extension GL_EXT_nonuniform_qualifier : require 

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragUv;
layout(location = 2) flat in uint imageIndex;

layout(binding = 2) uniform sampler samp;
layout(binding = 3) uniform texture2D textures[];

layout(location = 0) out vec4 outColor;

void main() 
{
    outColor = texture(sampler2D(textures[imageIndex], samp), fragUv) * fragColor;
}