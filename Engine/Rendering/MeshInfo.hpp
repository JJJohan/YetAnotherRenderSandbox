#pragma once

#include <glm/glm.hpp>
#include "Core/Colour.hpp"

namespace Engine::Rendering
{
	struct MeshInfo
	{
		glm::mat4 transform;
		Colour colour;
		size_t indexBufferIndex;
		size_t vertexBufferIndex;
		size_t diffuseImageIndex;
		size_t normalImageIndex;
	};
}