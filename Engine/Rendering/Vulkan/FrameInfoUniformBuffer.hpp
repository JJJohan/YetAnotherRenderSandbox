#pragma once

#include <glm/glm.hpp>

namespace Engine::Rendering::Vulkan
{
	// hard-coded for now
	struct FrameInfoUniformBuffer
	{
		glm::mat4 viewProj;
		glm::mat4 lightViewProj;
		glm::vec4 viewPos;
		glm::vec3 sunLightDir;
		uint32_t debugMode;
		glm::vec3 sunLightColour;
		float sunLightIntensity;
	};
}