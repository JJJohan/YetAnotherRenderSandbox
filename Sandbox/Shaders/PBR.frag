#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout(binding = 0) uniform FrameInfo
{
    mat4 viewProj;
	mat4 lightViewProj;
	vec4 viewPos;
	vec3 sunLightDir;
	uint debugMode;
	vec3 sunLightColor;
	float sunLightIntensity;
} frameInfo;

layout(location = 0) flat in uint fragDiffuseImageIndex;
layout(location = 1) flat in uint fragNormalImageIndex;
layout(location = 2) flat in uint fragMetallicRoughnessImageIndex;

layout(location = 3) in vec4 fragColor;
layout(location = 4) in vec2 fragUv;
layout(location = 5) in vec3 fragWorldPos;
layout(location = 6) in vec3 fragNormal;

layout(binding = 2) uniform sampler samp;
layout(binding = 3) uniform texture2D textures[];

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outWorldPos;
layout(location = 3) out vec2 outMetalRoughness;

vec3 unpackNormal()
{
	vec2 packed = texture(sampler2D(textures[fragNormalImageIndex], samp), fragUv).rg;
	packed = packed * 2.0 - 1.0;
	float z = sqrt(1.0 - packed.x * packed.x - packed.y * packed.y);
	vec3 tangentNormal = normalize(vec3(packed, z));

	vec3 q1 = dFdx(fragWorldPos);
	vec3 q2 = dFdy(fragWorldPos);
	vec2 st1 = dFdx(fragUv);
	vec2 st2 = dFdy(fragUv);

	vec3 N = normalize(fragNormal);
	vec3 T = normalize(q1 * st2.t - q2 * st1.t);
	vec3 B = -normalize(cross(N, T));
	mat3 TBN = mat3(T, B, N);

	return normalize(TBN * tangentNormal);
}

void main()
{
	vec4 baseColor = texture(sampler2D(textures[fragDiffuseImageIndex], samp), fragUv) * fragColor;
	if (baseColor.a < 0.5) discard;

	vec3 normal = unpackNormal();
	vec2 metalRoughness = texture(sampler2D(textures[fragMetallicRoughnessImageIndex], samp), fragUv).rg;

	outAlbedo = baseColor;
	outNormal = vec4(normal, 1.0);
	outWorldPos = vec4(fragWorldPos, 1.0);
	outMetalRoughness = metalRoughness;
}