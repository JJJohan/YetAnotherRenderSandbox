#pragma once

#include <stdint.h>
#include <vector>
#include <memory>

namespace Engine
{
	class VertexData;

	class MeshOptimiser
	{
	public:
		static bool Optimise(std::vector<uint32_t>& indices, std::vector<std::unique_ptr<VertexData>>& vertexArrays);
	};
}