#pragma once

#include <vector>
#include <memory>
#include "Macros.hpp"

namespace Engine
{
	class VertexData;

	class TangentCalculator
	{
	public:
		EXPORT static void CalculateTangents(const VertexData& positions, const VertexData& normals, const VertexData& uvs,
			const std::vector<uint32_t>& indices, std::unique_ptr<VertexData>& tangentsResult, std::unique_ptr<VertexData>& bitangentsResult);
	};
}