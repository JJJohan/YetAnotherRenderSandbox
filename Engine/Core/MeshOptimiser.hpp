#pragma once

#include <stdint.h>
#include <vector>
#include <memory>

namespace Engine::Rendering
{
	class VertexData;
}

namespace Engine
{
	class MeshOptimiser
	{
	public:
		static bool Optimise(std::vector<uint32_t>& indices, std::vector<std::unique_ptr<Engine::Rendering::VertexData>>& vertexArrays);
	};
}