#pragma once

#include <glm/glm.hpp>
#include "Core/Macros.hpp"
#include "Core/Colour.hpp"
#include "Core/Image.hpp"
#include "VertexData.hpp"
#include "MeshInfo.hpp"
#include <vector>
#include <stack>
#include <mutex>
#include <unordered_map>

namespace Engine::Rendering
{
	class Shader;

	class SceneManager
	{
	public:
		SceneManager();

		EXPORT uint32_t CreateMesh(
			const std::vector<VertexData>& vertexData,
			const std::vector<uint32_t>& indices,
			const glm::mat4& transform,
			const Colour& colour = {},
			std::shared_ptr<Image> image = nullptr);

		EXPORT void Build();

		EXPORT uint32_t CreateFromOBJ(const std::string& filePath,
			const glm::mat4& transform, const Colour& colour = {}, std::shared_ptr<Image> image = nullptr);

	protected:
		std::stack<uint32_t> m_recycledIds;
		std::vector<bool> m_active;
		std::mutex m_creationMutex;
		uint32_t m_meshCapacity;
		bool m_build;

		Shader* m_shader;
		std::vector<std::vector<std::unique_ptr<VertexData>>> m_vertexDataArrays;
		std::vector<std::unique_ptr<std::vector<uint32_t>>> m_indexArrays;
		std::vector<MeshInfo> m_meshInfos;
		std::vector<std::shared_ptr<Image>> m_images;
		std::vector<uint32_t> m_vertexBufferOffsets;
		std::vector<uint32_t> m_indexBufferOffsets;

	private:
		std::unordered_map<uint64_t, size_t> m_imageHashTable;
		std::unordered_map<uint64_t, size_t> m_vertexDataHashTable;
		std::unordered_map<uint64_t, size_t> m_indexDataHashTable;
	};
}