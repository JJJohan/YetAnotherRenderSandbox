#pragma once

#include <glm/glm.hpp>

namespace Engine::Rendering::Vulkan
{
	// hard-coded for now
	struct LightUniformBuffer
	{
		float cascadeSplits[4];
		glm::mat4 cascadeMatrices[4];
		glm::vec3 sunLightColour;
		float sunLightIntensity;
		glm::vec3 sunLightDir;
		float padding2;
	};
}