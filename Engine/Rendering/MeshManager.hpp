#pragma once

#include <glm/glm.hpp>
#include "Core/Macros.hpp"
#include "Core/Colour.hpp"
#include "Core/Image.hpp"
#include "VertexData.hpp"
#include <vector>
#include <stack>
#include <mutex>

namespace Engine::Rendering
{
	class Shader;

	enum class MeshUpdateFlagBits : uint8_t
	{
		None = 0,
		VertexData = 1 << 0,
		Indices = 1 << 1,
		Uniforms = 1 << 2,
		Image = 1 << 3,
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
		MeshManager();

		EXPORT uint32_t CreateMesh(const Shader* shader,
			const std::vector<VertexData>& vertexData,
			const std::vector<uint32_t>& indices,
			const glm::mat4& transform,
			const Colour& colour = {},
			std::shared_ptr<Image> image = nullptr);

		EXPORT uint32_t CreateFromOBJ(const Shader* shader, const std::string& filePath,
			const glm::mat4& transform, const Colour& colour = {}, std::shared_ptr<Image> image = nullptr);

		EXPORT virtual void DestroyMesh(uint32_t id);

		EXPORT void SetVertexData(uint32_t id, uint32_t slot, const VertexData& vertexData);
		EXPORT const VertexData& GetVertexData(uint32_t id, uint32_t slot) const;

		EXPORT void SetIndices(uint32_t id, const std::vector<uint32_t>& indices);
		EXPORT const std::vector<uint32_t>& GetIndices(uint32_t id) const;

		EXPORT void SetColour(uint32_t id, const Colour& colour);
		EXPORT const Colour& GetColour(uint32_t id) const;

		EXPORT void SetTransform(uint32_t id, const glm::mat4& transform);
		EXPORT const glm::mat4& GetTransform(uint32_t id) const;

		EXPORT void SetImage(uint32_t id, std::shared_ptr<Image> image); // released on load

		virtual void IncrementSize();

	protected:
		std::stack<uint32_t> m_recycledIds;
		std::vector<bool> m_active;
		std::mutex m_creationMutex;
		std::vector<MeshUpdateFlagBits> m_updateFlags;
		std::vector<uint8_t> m_vertexUpdateFlags;
		uint32_t m_meshCapacity;

		std::vector<std::vector<VertexData>> m_vertexDataArrays;
		std::vector<std::vector<uint32_t>> m_indexArrays;
		std::vector<const Shader*> m_shaders;
		std::vector<Colour> m_colours;
		std::vector<glm::mat4> m_transforms;
		std::vector<std::shared_ptr<Image>> m_images;
	};
}