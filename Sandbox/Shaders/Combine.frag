#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_samplerless_texture_functions : require

layout(binding = 0) uniform FrameInfo
{
    mat4 viewProj;
    mat4 prevViewProj;
    mat4 view;
	vec4 viewPos;
	vec2 viewSize;
	vec2 jitter;
} frameInfo;

layout(binding = 1) uniform LightData
{
	vec4 cascadeSplits;
    mat4 cascadeMatrices[4];
	vec3 sunLightColor;
	float sunLightIntensity;
	vec3 sunLightDir;
} lightData;

layout(binding = 2) uniform sampler samp;
layout(binding = 3) uniform texture2D textures[];
layout(binding = 4) uniform sampler shadowSampler;
layout(binding = 5) uniform texture2DArray shadowMap;

layout(location = 0) in vec2 fragUv;

#include "brdf.glsl"
#include "tonemapping.glsl"

layout(location = 0) out vec4 outColor;

layout (constant_id = 0) const uint debugMode = 0U;
layout (constant_id = 1) const uint shadowsEnabled = 1U;

const mat4 biasMat = mat4(
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0
);

float textureProj(vec4 shadowCoord, vec2 offset, uint cascadeIndex)
{
	if (shadowCoord.z > -1.0 && shadowCoord.z < 1.0)
	{
		float dist = texture(sampler2DArray(shadowMap, shadowSampler), vec3(shadowCoord.st + offset, cascadeIndex)).r;
		if (shadowCoord.w > 0 && dist < shadowCoord.z)
		{
			return 1.0;
		}
	}

	return 0.0;
}

float filterPCF(vec4 sc, uint cascadeIndex)
{
	ivec2 texDim = textureSize(shadowMap, 0).xy;
	float scale = 0.75;
	float dx = scale * 1.0 / float(texDim.x);
	float dy = scale * 1.0 / float(texDim.y);

	float shadowFactor = 0.0;
	int count = 0;
	int range = 1;

	for (int x = -range; x <= range; x++)
	{
		for (int y = -range; y <= range; y++)
		{
			shadowFactor += textureProj(sc, vec2(dx * x, dy * y), cascadeIndex);
			count++;
		}
	}

	return shadowFactor / count;
}

float ShadowCalculation(vec3 worldPos, float viewDepth)
{
	// Get cascade index for the current fragment's view position
	uint cascadeIndex = 0;
	for(uint i = 0; i < 3; ++i)
	{
		if(viewDepth < lightData.cascadeSplits[i])
		{
			cascadeIndex = i + 1;
		}
	}

	// Depth compare for shadowing
	vec4 shadowCoord = (biasMat * lightData.cascadeMatrices[cascadeIndex]) * vec4(worldPos, 1.0);

	return filterPCF(shadowCoord / shadowCoord.w, cascadeIndex);
}

void main()
{
	vec4 baseColor = texture(sampler2D(textures[0], samp), fragUv);
	vec3 normal = texture(sampler2D(textures[1], samp), fragUv).rgb;
	vec4 worldPosAndViewDepth = texture(sampler2D(textures[2], samp), fragUv);
	vec2 metalRoughness = texture(sampler2D(textures[3], samp), fragUv).rg;
	vec3 worldPos = worldPosAndViewDepth.xyz - frameInfo.viewPos.xyz;
	float viewDepth = -worldPosAndViewDepth.w;

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

	vec3 lightColor = lightData.sunLightColor * lightData.sunLightIntensity;
	vec3 lightDir = -lightData.sunLightDir;

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

	vec3 color;
	if (shadowsEnabled != 0U)
	{
		// Calculate shadow
		float shadow = ShadowCalculation(worldPos, viewDepth);

		// Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
		color = (1.0 - shadow) * NdotL * lightColor * (diffuseContrib + specContrib);
	}
	else
	{
		color = NdotL * lightColor * (diffuseContrib + specContrib);
	}

	// Calculate lighting contribution from image based lighting source (IBL)
	//color += getIBLContribution(pbrInputs, n, reflection);
	color += baseColor.rgb * 0.1; // static ambient

	switch (debugMode)
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

		case 5: // cascade index

		uint cascadeIndex = 0U;
		for(uint i = 0; i < 3; ++i)
		{
			if(viewDepth < lightData.cascadeSplits[i])
			{
				cascadeIndex = i + 1;
			}
		}

		if (cascadeIndex == 0U)
			outColor = vec4(color, 1.0) * vec4(1, 0.5, 0.5, 1);
		else if (cascadeIndex == 1U)
			outColor = vec4(color, 1.0) * vec4(1, 1, 0.5, 1);
		else if (cascadeIndex == 2U)
			outColor = vec4(color, 1.0) * vec4(0.5, 1, 0.5, 1);
		else
			outColor = vec4(color, 1.0) * vec4(0.5, 0.5, 1, 1);

			break;

		default:
			outColor = vec4(color, 1.0);
			break;
	}
}