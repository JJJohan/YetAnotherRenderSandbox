#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout(binding = 0) uniform FrameInfo
{
    mat4 viewProj;
    mat4 prevViewProj;
    mat4 view;
	vec4 viewPos;
	vec2 viewSize;
	vec2 jitter;
	uint debugMode;
} frameInfo;

layout(location = 0) flat in uint fragDiffuseImageIndex;
layout(location = 1) flat in uint fragNormalImageIndex;
layout(location = 2) flat in uint fragMetallicRoughnessImageIndex;

layout(location = 3) in vec4 fragColor;
layout(location = 4) in vec2 fragUv;
layout(location = 5) in vec4 fragPrevPos;
layout(location = 6) in vec4 fragPos;
layout(location = 7) in vec4 fragWorldPosAndViewDepth;
layout(location = 8) in vec3 fragNormal;

layout(binding = 2) uniform sampler samp;
layout(binding = 3) uniform texture2D textures[];

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outWorldPosAndViewDepth;
layout(location = 3) out vec2 outMetalRoughness;
layout(location = 4) out vec2 outVelocity;

vec3 unpackNormal()
{
	vec2 packed = texture(sampler2D(textures[fragNormalImageIndex], samp), fragUv).rg;
	packed = packed * 2.0 - 1.0;
	float z = sqrt(1.0 - packed.x * packed.x - packed.y * packed.y);
	vec3 tangentNormal = normalize(vec3(packed, z));

	vec3 q1 = dFdx(fragWorldPosAndViewDepth.xyz);
	vec3 q2 = dFdy(fragWorldPosAndViewDepth.xyz);
	vec2 st1 = dFdx(fragUv);
	vec2 st2 = dFdy(fragUv);

	vec3 N = normalize(fragNormal);
	vec3 T = normalize(q1 * st2.t - q2 * st1.t);
	vec3 B = -normalize(cross(N, T));
	mat3 TBN = mat3(T, B, N);

	return normalize(TBN * tangentNormal);
}

vec2 getVelocity()
{
    vec2 oldPos = fragPrevPos.xy / fragPrevPos.w;
    vec2 newPos = fragPos.xy / fragPos.w;
    return newPos - oldPos;
}

void main()
{
	vec4 baseColor = texture(sampler2D(textures[fragDiffuseImageIndex], samp), fragUv) * fragColor;
	if (baseColor.a < 0.5) discard;

	vec3 normal = unpackNormal();
	vec2 metalRoughness = texture(sampler2D(textures[fragMetallicRoughnessImageIndex], samp), fragUv).rg;

	outAlbedo = baseColor;
	outNormal = vec4(normal, 1.0);
	outWorldPosAndViewDepth = fragWorldPosAndViewDepth;
	outMetalRoughness = metalRoughness;
	outVelocity = getVelocity();
}