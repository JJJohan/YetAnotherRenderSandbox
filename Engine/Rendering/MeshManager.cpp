#include "MeshManager.hpp"
#include "Shader.hpp"

namespace Engine::Rendering
{
	uint32_t MeshManager::CreateMesh(const Shader* shader,
		const std::vector<glm::vec3>& positions,
		const std::vector<Colour>& vertexColours,
		const std::vector<uint32_t>& indices, 
		const Colour& colour, 
		const glm::mat4& transform)
	{
		uint32_t id;
		if (!m_recycledIds.empty())
		{
			id = m_recycledIds.top();
			m_recycledIds.pop();
		}
		else
		{
			static std::atomic<uint32_t> id_counter {0};
			IncrementSize();
			id = id_counter++;
		}

		m_positionArrays[id] = positions;
		m_vertexColourArrays[id] = vertexColours;
		m_indexArrays[id] = indices;
		m_colours[id] = colour;
		m_transforms[id] = transform;
		m_updateFlags[id] = MeshUpdateFlagBits::None; // Creation will update everything.

		m_active[id] = true;

		return id;
	}

	void MeshManager::DestroyMesh(uint32_t id)
	{
		m_active[id] = false;

		m_positionArrays[id] = {};
		m_vertexColourArrays[id] = {};
		m_indexArrays[id] = {};
		m_updateFlags[id] = MeshUpdateFlagBits::None;

		m_recycledIds.push(id);
	}

	void MeshManager::IncrementSize()
	{
		m_active.push_back(false);

		m_positionArrays.push_back({});
		m_vertexColourArrays.push_back({});
		m_indexArrays.push_back({});
		m_colours.push_back({});
		m_transforms.push_back({});
		m_updateFlags.push_back(MeshUpdateFlagBits::None);
	}

	void MeshManager::SetPositions(uint32_t id, const std::vector<glm::vec3>& positions)
	{
		m_positionArrays[id] = positions;
		m_updateFlags[id] = m_updateFlags[id] | MeshUpdateFlagBits::Positions;
	}

	const std::vector<glm::vec3>& MeshManager::GetPositions(uint32_t id) const
	{
		return m_positionArrays[id];
	}

	void MeshManager::SetVertexColours(uint32_t id, const std::vector<Colour>& colours)
	{
		m_vertexColourArrays[id] = colours;
		m_updateFlags[id] = m_updateFlags[id] | MeshUpdateFlagBits::VertexColours;
	}

	const std::vector<Colour>& MeshManager::GetVertexColours(uint32_t id) const
	{
		return m_vertexColourArrays[id];
	}

	void MeshManager::SetIndices(uint32_t id, const std::vector<uint32_t>& indices)
	{
		m_indexArrays[id] = indices;
		m_updateFlags[id] = m_updateFlags[id] | MeshUpdateFlagBits::Indices;
	}

	const std::vector<uint32_t>& MeshManager::GetIndices(uint32_t id) const
	{
		return m_indexArrays[id];
	}

	void MeshManager::SetColour(uint32_t id, const Colour& colour)
	{
		m_colours[id] = colour;
		m_updateFlags[id] = m_updateFlags[id] | MeshUpdateFlagBits::Colour;
	}

	const Colour& MeshManager::GetColour(uint32_t id) const
	{
		return m_colours[id];
	}

	void MeshManager::SetTransform(uint32_t id, const glm::mat4& transform)
	{
		m_transforms[id] = transform;
		m_updateFlags[id] = m_updateFlags[id] | MeshUpdateFlagBits::Transform;
	}

	const glm::mat4& MeshManager::GetTransform(uint32_t id) const
	{
		return m_transforms[id];
	}
}