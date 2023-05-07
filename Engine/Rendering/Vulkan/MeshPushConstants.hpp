#pragma once

#include <glm/glm.hpp>
#include "Core/Colour.hpp"

namespace Engine::Rendering::Vulkan
{
	struct MeshPushConstants
	{
		glm::mat4 transform;
		glm::vec4 colour; // map uint to vec4?
	};
}