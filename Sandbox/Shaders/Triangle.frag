#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout(binding = 0) uniform FrameInfo
{
    mat4 viewProj;
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

layout(location = 0) out vec4 outColor;

#include "tonemapping.glsl"
#include "brdf.glsl"

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

	vec2 metalRoughness = texture(sampler2D(textures[fragMetallicRoughnessImageIndex], samp), fragUv).rg;
	vec3 f0 = vec3(0.04);

	vec3 diffuseColor = baseColor.rgb * (vec3(1.0) - f0);
	diffuseColor *= 1.0 - metalRoughness.x;

	float alphaRoughness = metalRoughness.y * metalRoughness.y;

	vec3 specularColor = mix(f0, baseColor.rgb, metalRoughness.x);

	// Compute reflectance.
	float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);

	// For typical incident reflectance range (between 4% to 100%) set the grazing reflectance to 100% for typical fresnel effect.
	// For very low reflectance range on highly diffuse objects (below 4%), incrementally reduce grazing reflecance to 0%.
	float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
	vec3 specularEnvironmentR0 = specularColor.rgb;
	vec3 specularEnvironmentR90 = vec3(1.0, 1.0, 1.0) * reflectance90;

	vec3 lightColor = frameInfo.sunLightColor * frameInfo.sunLightIntensity;
	vec3 lightDir = -frameInfo.sunLightDir;

	vec3 n = unpackNormal();
	vec3 v = normalize(frameInfo.viewPos.xyz - fragWorldPos);    // Vector from surface point to camera
	vec3 l = normalize(lightDir);     // Vector from surface point to light
	vec3 h = normalize(l+v);                        // Half vector between both l and v
	vec3 reflection = -normalize(reflect(v, n));
	reflection.y *= -1.0f;

	float NdotL = clamp(dot(n, l), 0.001, 1.0);
	float NdotV = clamp(abs(dot(n, v)), 0.001, 1.0);
	float NdotH = clamp(dot(n, h), 0.0, 1.0);
	float LdotH = clamp(dot(l, h), 0.0, 1.0);
	float VdotH = clamp(dot(v, h), 0.0, 1.0);

	PBRInfo pbrInputs = PBRInfo(
		NdotL,
		NdotV,
		NdotH,
		LdotH,
		VdotH,
		metalRoughness.y,
		metalRoughness.x,
		specularEnvironmentR0,
		specularEnvironmentR90,
		alphaRoughness,
		diffuseColor,
		specularColor
	);

	// Calculate the shading terms for the microfacet specular shading model
	vec3 F = specularReflection(pbrInputs);
	float G = geometricOcclusion(pbrInputs);
	float D = microfacetDistribution(pbrInputs);

	// Calculation of analytical lighting contribution
	vec3 diffuseContrib = (1.0 - F) * diffuse(pbrInputs);
	vec3 specContrib = F * G * D / (4.0 * NdotL * NdotV);
	// Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
	vec3 color = NdotL * lightColor * (diffuseContrib + specContrib);

	// Calculate lighting contribution from image based lighting source (IBL)
	//color += getIBLContribution(pbrInputs, n, reflection);

	// TODO: Shader constants, not uniform!
	switch (frameInfo.debugMode)
	{
		case 1: // Albedo
			outColor = vec4(baseColor.rgb, 1);
			break;

		case 2: // Metalness
			outColor = vec4(vec3(metalRoughness.x), 1);
			break;

		case 3: // Roughness
			outColor = vec4(vec3(metalRoughness.y), 1);
			break;

		default:
			//color = tonemapUncharted2(color);
			outColor = vec4(color, baseColor.a);
			break;
	}
}