#pragma once

#include <glm/glm.hpp>
#include "Core/Macros.hpp"
#include "Core/Colour.hpp"
#include <vector>

namespace Engine::Rendering
{
	class Mesh
	{
	public:
		EXPORT Mesh();
		EXPORT void Clear();

		EXPORT void SetPositions(const std::vector<glm::vec3>& positions);
		EXPORT const std::vector<glm::vec3>& GetPositions() const;

		EXPORT void SetColours(const std::vector<Colour>& colours);
		EXPORT const std::vector<Colour>& GetColours() const;

		EXPORT void SetIndices(const std::vector<uint32_t>& indices);
		EXPORT const std::vector<uint32_t>& GetIndices() const;

		const uint32_t Id;

	protected:
		std::vector<glm::vec3> m_positions;
		std::vector<Colour> m_colours;
		std::vector<uint32_t> m_indices;
	};
}