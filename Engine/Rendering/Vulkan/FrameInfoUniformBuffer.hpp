#pragma once

#include <glm/glm.hpp>

namespace Engine::Rendering::Vulkan
{
	// hard-coded for now
	struct FrameInfoUniformBuffer
	{
		glm::mat4 viewProj;
		glm::mat4 view;
		glm::vec4 viewPos;
		uint32_t debugMode;
	};
}