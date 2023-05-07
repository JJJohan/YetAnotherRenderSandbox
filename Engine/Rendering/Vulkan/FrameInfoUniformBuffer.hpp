#pragma once

#include <glm/glm.hpp>

namespace Engine::Rendering::Vulkan
{
	// hard-coded for now
	struct FrameInfoUniformBuffer
	{
		glm::mat4 viewProj;
	};
}