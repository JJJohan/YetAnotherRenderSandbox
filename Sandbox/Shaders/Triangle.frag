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

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragUv;
layout(location = 2) flat in uint fragDiffuseImageIndex;
layout(location = 3) flat in uint fragNormalImageIndex;

layout(location = 4) in vec3 fragTangentLightPos;
layout(location = 5) in vec3 fragTangentViewPos;
layout(location = 6) in vec3 fragTangentFragPos;

layout(binding = 2) uniform sampler samp;
layout(binding = 3) uniform texture2D textures[];

layout(location = 0) out vec4 outColor;

void main() 
{
    vec4 surfaceColor = texture(sampler2D(textures[fragDiffuseImageIndex], samp), fragUv) * fragColor;
	
	if (surfaceColor.a <= 0.0) discard;
	
	vec3 normal = texture(sampler2D(textures[fragNormalImageIndex], samp), fragUv).rgb;
    normal = normalize(normal * 2.0 - 1.0);  

    // ambient
    vec3 ambient = 0.1 * surfaceColor.rgb;
	
    // diffuse
    vec3 lightDir = normalize(fragTangentLightPos - fragTangentFragPos);
	float diff = max(dot(lightDir, normal), 0.0);
    vec3 diffuse = diff * frameInfo.sunLightColor.rgb * surfaceColor.rgb;
	
    // specular
    vec3 viewDir = normalize(fragTangentViewPos - fragTangentFragPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    vec3 halfwayDir = normalize(lightDir + viewDir);  
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
    vec3 specular = vec3(0.2) * spec;

	// Specular is currently skipped since I'm just using a single directional light which can look a bit odd.
    vec3 linearColor = ambient + diffuse;// + specular;
	
	vec3 gamma = vec3(1.0 / 2.2);
    outColor = vec4(pow(linearColor, gamma), surfaceColor.a);
}