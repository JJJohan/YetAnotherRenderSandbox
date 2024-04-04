#pragma once

#include <glm/glm.hpp>

namespace Engine::Rendering
{
	// hard-coded for now
	struct alignas(16) LightUniformBuffer
	{
		float cascadeSplits[4];
		glm::mat4 cascadeMatrices[4];
		glm::vec3 sunLightColour;
		float sunLightIntensity;
		glm::vec3 sunLightDir;
	};
}