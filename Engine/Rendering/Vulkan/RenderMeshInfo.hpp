#pragma once

#include <glm/glm.hpp>
#include "Core/Colour.hpp"

namespace Engine::Rendering
{
	struct alignas(16) RenderMeshInfo
	{
		glm::mat4 transform;
		glm::vec4 colour; // map uint to vec4?
		uint32_t imageIndex;
	};
}