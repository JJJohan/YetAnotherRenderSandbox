#pragma once

#include <glm/glm.hpp>

namespace Engine::Rendering::Vulkan
{
	// hard-coded for now
	struct FrameInfoUniformBuffer
	{
		glm::mat4 viewProj;
		glm::vec4 viewPos;
		glm::vec4 ambientColour;
		glm::vec4 sunLightDir;
		glm::vec4 sunLightColour;
	};
}