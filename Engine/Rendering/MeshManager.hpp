#pragma once

#include <glm/glm.hpp>
#include "Core/Macros.hpp"
#include "Core/Colour.hpp"
#include <vector>
#include <stack>
#include <mutex>

namespace Engine::Rendering
{
	class Shader;

	enum class MeshUpdateFlagBits : uint8_t
	{
		None = 0,
		Positions = 1 << 0,
		VertexColours = 1 << 1,
		Indices = 1 << 2,
		Colour = 1 << 3,
		Transform = 1 << 4,
		All = 0xFF
	};

	inline MeshUpdateFlagBits operator|(MeshUpdateFlagBits lhs, MeshUpdateFlagBits rhs)
	{
		return static_cast<MeshUpdateFlagBits>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
	}

	inline MeshUpdateFlagBits operator&(MeshUpdateFlagBits lhs, MeshUpdateFlagBits rhs)
	{
		return static_cast<MeshUpdateFlagBits>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
	}

	class MeshManager
	{
	public:
		EXPORT virtual uint32_t CreateMesh(const Shader* shader,
			const std::vector<glm::vec3>& positions,
			const std::vector<Colour>& vertexColours,
			const std::vector<uint32_t>& indices,
			const Colour& colour,
			const glm::mat4& transform);

		EXPORT virtual void DestroyMesh(uint32_t id);

		EXPORT void SetPositions(uint32_t id, const std::vector<glm::vec3>& positions);
		EXPORT const std::vector<glm::vec3>& GetPositions(uint32_t id) const;

		EXPORT void SetVertexColours(uint32_t id, const std::vector<Colour>& colours);
		EXPORT const std::vector<Colour>& GetVertexColours(uint32_t id) const;

		EXPORT void SetIndices(uint32_t id, const std::vector<uint32_t>& indices);
		EXPORT const std::vector<uint32_t>& GetIndices(uint32_t id) const;

		EXPORT void SetColour(uint32_t id, const Colour& colour);
		EXPORT const Colour& GetColour(uint32_t id) const;

		EXPORT void SetTransform(uint32_t id, const glm::mat4& transform);
		EXPORT const glm::mat4& GetTransform(uint32_t id) const;

		virtual void IncrementSize();

	protected:
		std::stack<uint32_t> m_recycledIds;
		std::vector<bool> m_active;
		std::mutex m_creationMutex;
		std::vector<MeshUpdateFlagBits> m_updateFlags;

		std::vector<std::vector<glm::vec3>> m_positionArrays;
		std::vector<std::vector<Colour>> m_vertexColourArrays;
		std::vector<std::vector<uint32_t>> m_indexArrays;
		std::vector<Colour> m_colours;
		std::vector<glm::mat4> m_transforms;
	};
}