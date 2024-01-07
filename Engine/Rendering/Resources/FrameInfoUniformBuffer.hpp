#pragma once

#include <glm/glm.hpp>

namespace Engine::Rendering
{
	// hard-coded for now
	struct FrameInfoUniformBuffer
	{
		glm::mat4 viewProj;
		glm::mat4 prevViewProj;
		glm::mat4 view;
		glm::vec4 viewPos;
		glm::vec2 viewSize;
		glm::vec2 jitter;
	};
}