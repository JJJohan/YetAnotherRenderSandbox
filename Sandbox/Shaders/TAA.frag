#version 460
#extension GL_EXT_samplerless_texture_functions : require

#include "catmullsample.glsl"

layout(binding = 0) uniform sampler linearSampler;
layout(binding = 1) uniform sampler nearestSampler;
layout(binding = 2) uniform texture2D inputImage;
layout(binding = 3) uniform texture2D previousImage;
layout(binding = 4) uniform texture2D velocityImage;
layout(binding = 5) uniform texture2D depthImage;

layout(location = 0) in vec2 fragUv;

layout(location = 0) out vec4 outColor;

float mitchell(float x)
{
  float t = abs(x);
  float t2 = t * t;
  float t3 = t * t * t;

  if (t < 1)
    return 7.0 / 6.0 * t3 + -2.0 * t2 + 8.0 / 9.0;
  else if (t < 2)
    return -7.0 / 18.0 * t3 + 2.0 * t2 - 10.0 / 3.0 * t + 16.0 / 9.0;
  else
    return 0.0;
}

vec3 clip_aabb(vec3 aabb_min, vec3 aabb_max, vec3 p, vec3 q)
{
	// note: only clips towards aabb center (but fast!)
	vec3 p_clip = 0.5 * (aabb_max + aabb_min);
	vec3 e_clip = 0.5 * (aabb_max - aabb_min) + 0.00000001;

	vec3 v_clip = q - p_clip;
	vec3 v_unit = v_clip.xyz / e_clip;
	vec3 a_unit = abs(v_unit);
	float ma_unit = max(a_unit.x, max(a_unit.y, a_unit.z));

	if (ma_unit > 1.0)
		return p_clip + v_clip / ma_unit;
	else
		return q;// point inside aabb
}

float luminance(vec3 rgb)
{
    return dot(rgb, vec3(0.0396819152, 0.458021790, 0.00609653955));
}

void main()
{
	vec2 texDimensions = textureSize(inputImage, 0);
	vec2 texelSize = vec2(1.0) / texDimensions;
	vec3 sourceSampleTotal = vec3(0.0);
	float sourceSampleWeight = 0.0;
	vec3 neighborhoodMin = vec3(10000.0);
	vec3 neighborhoodMax = vec3(-10000.0);
	vec3 m1 = vec3(0.0);
	vec3 m2 = vec3(0.0);
	float closestDepth = 0.0;
	vec2 closestDepthPixelPosition = vec2(0, 0);

	for (int x = -1; x <= 1; x++)
	{
		for (int y = -1; y <= 1; y++)
		{
			vec2 pixelPosition = fragUv + vec2(x, y) * texelSize;

			vec3 neighbor = max(vec3(0.0), texture(sampler2D(inputImage, nearestSampler), pixelPosition).rgb);
			float subSampleDistance = length(vec2(x, y));
			float subSampleWeight = mitchell(subSampleDistance);

			sourceSampleTotal += neighbor * subSampleWeight;
			sourceSampleWeight += subSampleWeight;

			neighborhoodMin = min(neighborhoodMin, neighbor);
			neighborhoodMax = max(neighborhoodMax, neighbor);

			m1 += neighbor;
			m2 += neighbor * neighbor;

			float currentDepth = texture(sampler2D(depthImage, nearestSampler), pixelPosition).r;
			if (currentDepth > closestDepth)
			{
				closestDepth = currentDepth;
				closestDepthPixelPosition = pixelPosition;
			}
		}
	}

	vec2 motionVector = texture(sampler2D(velocityImage, nearestSampler), closestDepthPixelPosition).xy * vec2(0.5, 0.5);
	vec2 historyTexCoord = fragUv - motionVector;
	vec3 sourceSample = sourceSampleTotal / sourceSampleWeight;
	vec3 historySample = SampleTextureCatmullRom(previousImage, linearSampler, historyTexCoord, texDimensions).rgb;

	float oneDividedBySampleCount = 1.0 / 9.0;
	float gamma = 1.0;
	vec3 mu = m1 * oneDividedBySampleCount;
	vec3 sigma = sqrt(abs((m2 * oneDividedBySampleCount) - (mu * mu)));
	vec3 minc = mu - gamma * sigma;
	vec3 maxc = mu + gamma * sigma;

	historySample = clip_aabb(minc, maxc, clamp(historySample, neighborhoodMin, neighborhoodMax), historySample);

	float sourceWeight = 0.05;
	float historyWeight = 1.0 - sourceWeight;
	vec3 compressedSource = sourceSample * (1.0 / (max(max(sourceSample.r, sourceSample.g), sourceSample.b) + 1.0));
	vec3 compressedHistory = historySample * (1.0 / (max(max(historySample.r, historySample.g), historySample.b) + 1.0));
	float luminanceSource = luminance(compressedSource);
	float luminanceHistory = luminance(compressedHistory);

	sourceWeight *= 1.0 / (1.0 + luminanceSource);
	historyWeight *= 1.0 / (1.0 + luminanceHistory);

	vec3 result = (sourceSample * sourceWeight + historySample * historyWeight) / max(sourceWeight + historyWeight, 0.00001);

	outColor = vec4(result, 1.0);
}