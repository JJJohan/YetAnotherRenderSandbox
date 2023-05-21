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


layout(location = 0) flat in uint fragDiffuseImageIndex;
layout(location = 1) flat in uint fragNormalImageIndex;
layout(location = 2) flat in uint fragMetallicRoughnessImageIndex;

layout(location = 3) in vec4 fragColor;
layout(location = 4) in vec2 fragUv;
layout(location = 5) in vec3 fragTangentLightPos;
layout(location = 6) in vec3 fragTangentViewPos;
layout(location = 7) in vec3 fragTangentFragPos;

layout(binding = 2) uniform sampler samp;
layout(binding = 3) uniform texture2D textures[];

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 unpackNormal()
{
	vec2 N = texture(sampler2D(textures[fragNormalImageIndex], samp), fragUv).rg;
	N = N * 2.0 - 1.0;
	float z = sqrt(1.0 - N.x * N.x - N.y * N.y);
	return normalize(vec3(N, z));
}

void main()
{
    vec4 surfaceColor = texture(sampler2D(textures[fragDiffuseImageIndex], samp), fragUv) * fragColor;
	if (surfaceColor.a <= 0.0) discard;

    vec3 albedo     = pow(surfaceColor.rgb, vec3(2.2));
    vec2 metallicRoughness = texture(sampler2D(textures[fragMetallicRoughnessImageIndex], samp), fragUv).rg;

	vec3 N = unpackNormal();
	vec3 V = normalize(fragTangentViewPos - fragTangentFragPos);

    // calculate reflectance at normal incidence; if dia-electric (like plastic) use F0
    // of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallicRoughness.x);

    // reflectance equation
    vec3 Lo = vec3(0.0);
    //for(int i = 0; i < 4; ++i)
    {
        // calculate per-light radiance
        //vec3 L = normalize(lightPositions[i] - fragWorldPos);
        vec3 L = normalize(fragTangentLightPos - fragTangentFragPos);
        vec3 H = normalize(V + L);
        //float distance = length(lightPositions[i] - fragWorldPos);
        //float attenuation = 1.0 / (distance * distance);
		//vec3 radiance = lightColors[i] * attenuation;
		vec3 radiance = frameInfo.sunLightColor.rgb * frameInfo.sunLightColor.a;

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, metallicRoughness.y);
        float G   = GeometrySmith(N, V, L, metallicRoughness.y);
        vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 numerator    = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
        //vec3 specular = numerator / denominator;
		vec3 specular = vec3(0.0);

        // kS is equal to Fresnel
        vec3 kS = F;
        // for energy conservation, the diffuse and specular light can't
        // be above 1.0 (unless the surface emits light); to preserve this
        // relationship the diffuse component (kD) should equal 1.0 - kS.
        vec3 kD = vec3(1.0) - kS;
        // multiply kD by the inverse metalness such that only non-metals
        // have diffuse lighting, or a linear blend if partly metal (pure metals
        // have no diffuse light).
        kD *= 1.0 - metallicRoughness.x;

        // scale light by NdotL
        float NdotL = max(dot(N, L), 0.0);

        // add to outgoing radiance Lo
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

	// TODO: IBL
    vec3 ambient = vec3(0.03) * albedo;

    vec3 color = ambient + Lo;

    // HDR tonemapping
    color = color / (color + vec3(1.0));

    // gamma correct
    color = pow(color, vec3(1.0/2.2));

    outColor = vec4(color, surfaceColor.a);
}