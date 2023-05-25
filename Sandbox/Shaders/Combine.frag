#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_samplerless_texture_functions : require

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

layout(binding = 1) uniform sampler samp;
layout(binding = 2) uniform texture2D textures[];
layout(binding = 3) uniform sampler shadowSampler;
layout(binding = 4) uniform texture2D shadowMap;

layout(location = 0) in vec2 fragUv;

#include "brdf.glsl"
#include "tonemapping.glsl"

layout(location = 0) out vec4 outColor;

float textureProj(vec4 P, vec2 offset)
{
	vec4 shadowCoord = P / P.w;
	shadowCoord.st = shadowCoord.st * 0.5 + 0.5;

	if (shadowCoord.z > -1.0 && shadowCoord.z < 1.0)
	{
		float dist = texture(sampler2D(shadowMap, shadowSampler), vec2(shadowCoord.st + offset)).r;
		if (shadowCoord.w > 0.0 && dist < shadowCoord.z)
		{
			return 1.0;
		}
	}

	return 0.0;
}

float filterPCF(vec4 sc)
{
	ivec2 texDim = textureSize(shadowMap, 0).xy;
	float scale = 1.5;
	float dx = scale * 1.0 / float(texDim.x);
	float dy = scale * 1.0 / float(texDim.y);

	float shadowFactor = 0.0;
	int count = 0;
	int range = 1;

	for (int x = -range; x <= range; x++)
	{
		for (int y = -range; y <= range; y++)
		{
			shadowFactor += textureProj(sc, vec2(dx*x, dy*y));
			count++;
		}

	}
	return shadowFactor / count;
}

float ShadowCalculation(vec3 fragPos)
{
	vec4 shadowClip	= frameInfo.lightViewProj * vec4(fragPos, 1.0);
	return filterPCF(shadowClip);
}

void main()
{
	vec4 baseColor = texture(sampler2D(textures[0], samp), fragUv);
	vec3 normal = texture(sampler2D(textures[1], samp), fragUv).rgb;
	vec3 worldPos = texture(sampler2D(textures[2], samp), fragUv).rgb;
	vec2 metalRoughness = texture(sampler2D(textures[3], samp), fragUv).rg;

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

	vec3 n = normal;
	vec3 v = normalize(frameInfo.viewPos.xyz - worldPos);    // Vector from surface point to camera
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

    // Calculate shadow
    float shadow = ShadowCalculation(worldPos);

	// Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
	vec3 color = (1.0 - shadow) * NdotL * lightColor * (diffuseContrib + specContrib);

	// Calculate lighting contribution from image based lighting source (IBL)
	//color += getIBLContribution(pbrInputs, n, reflection);
	color += baseColor.rgb * 0.1; // static ambient

	switch (frameInfo.debugMode)
	{
		case 1: // Albedo
			outColor = vec4(baseColor.rgb, 1);
			break;

		case 2: // Normal
			outColor = vec4(normal, 1);
			break;

		case 3: // WorldPos
			outColor = vec4(worldPos, 1);
			break;

		case 4: // MetalRoughness
			outColor = vec4(metalRoughness, 0, 1);
			break;

		default:
			outColor = vec4(color, 1.0);
			break;
	}
}