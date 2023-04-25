#include "Mesh.hpp"
#include "Core/Utils.hpp"

namespace Engine::Rendering
{
	Mesh::Mesh()
		: m_positions()
		, m_colours()
		, m_indices()
		, Id(Utils::GetId())
	{
	}

	void Mesh::SetPositions(const std::vector<glm::vec3>& positions)
	{
		m_positions = positions;
	}

	const std::vector<glm::vec3>& Mesh::GetPositions() const
	{
		return m_positions;
	}

	void Mesh::SetColours(const std::vector<Colour>& colours)
	{
		m_colours = colours;
	}

	const std::vector<Colour>& Mesh::GetColours() const
	{
		return m_colours;
	}

	void Mesh::SetIndices(const std::vector<uint32_t>& indices)
	{
		m_indices = indices;
	}

	const std::vector<uint32_t>& Mesh::GetIndices() const
	{
		return m_indices;
	}

	void Mesh::Clear()
	{
		m_positions.clear();
		m_colours.clear();
		m_indices.clear();
	}
}