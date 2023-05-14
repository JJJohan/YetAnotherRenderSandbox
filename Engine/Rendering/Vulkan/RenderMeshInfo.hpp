#pragma once

#include <glm/glm.hpp>
#include "Core/Colour.hpp"

namespace Engine::Rendering
{
	struct alignas(16) RenderMeshInfo
	{
		glm::mat4 transform;
		glm::vec4 colour;

		uint32_t diffuseImageIndex;
		uint32_t normalImageIndex;
		uint32_t metallicRoughnessImageIndex;
	};
}