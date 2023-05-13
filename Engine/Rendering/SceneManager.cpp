#include "SceneManager.hpp"
#include "Shader.hpp"
#include "Core/Logging/Logger.hpp"
#include <filesystem>
#include "GltfLoader.hpp"
#include "TangentCalculator.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

using namespace Engine::Logging;

namespace Engine::Rendering
{
	SceneManager::SceneManager()
		:m_recycledIds()
		, m_active()
		, m_creationMutex()
		, m_meshCapacity()
		, m_shader()
		, m_vertexDataArrays()
		, m_indexArrays()
		, m_meshInfos()
		, m_images()
		, m_vertexDataHashTable()
		, m_imageHashTable()
		, m_build(false)
	{
	}

	uint32_t SceneManager::CreateMesh(const std::vector<VertexData>& vertexData,
		const std::vector<uint32_t>& indices,
		const glm::mat4& transform,
		const Colour& colour,
		std::shared_ptr<Image> diffuseImage,
		std::shared_ptr<Image> normalImage)
	{
		if (vertexData.empty())
		{
			Logger::Error("Empty vertex buffer vector not permitted.");
			return 0;
		}

		const std::lock_guard<std::mutex> lock(m_creationMutex);

		uint32_t id;
		if (!m_recycledIds.empty())
		{
			id = m_recycledIds.top();
			m_recycledIds.pop();
		}
		else
		{
			m_meshInfos.push_back({});
			m_active.push_back(false);
			id = m_meshCapacity++;
		}

		MeshInfo& meshInfo = m_meshInfos[id];
		meshInfo.transform = transform;
		meshInfo.colour = colour;

		uint64_t indexHash = Hash::CalculateHash(indices.data(), indices.size() * sizeof(uint32_t));
		const auto& indexResult = m_indexDataHashTable.find(indexHash);
		if (indexResult != m_indexDataHashTable.cend())
		{
			meshInfo.indexBufferIndex = indexResult->second;
		}
		else
		{
			m_indexDataHashTable[indexHash] = m_indexArrays.size();
			meshInfo.indexBufferIndex = m_indexArrays.size();
			m_indexArrays.emplace_back(std::make_unique<std::vector<uint32_t>>(indices));
		}

		uint64_t vertexHash = vertexData[0].GetHash(); // Only hash the first vertex buffer to keep things simple.

		const auto& vertexResult = m_vertexDataHashTable.find(vertexHash);
		if (vertexResult != m_vertexDataHashTable.cend())
		{
			meshInfo.vertexBufferIndex = vertexResult->second;
		}
		else
		{
			m_vertexDataHashTable[vertexHash] = m_vertexDataArrays.size();
			meshInfo.vertexBufferIndex = m_vertexDataArrays.size();

			std::vector<std::unique_ptr<VertexData>> localVertexData;
			localVertexData.reserve(vertexData.size() + 2);
			for (auto& vertices : vertexData)
				localVertexData.emplace_back(std::make_unique<VertexData>(vertices));

			// TODO: Clean up so this isn't done if not necessary, and buffer order isn't assumed.			
			std::unique_ptr<VertexData> tangents;
			std::unique_ptr<VertexData> bitangents;
			TangentCalculator::CalculateTangents(vertexData[0], vertexData[2], vertexData[1], indices, tangents, bitangents);

			localVertexData.emplace_back(std::move(tangents));
			localVertexData.emplace_back(std::move(bitangents));

			m_vertexDataArrays.push_back(std::move(localVertexData));
		}

		if (diffuseImage.get() != nullptr)
		{
			uint64_t imageHash = diffuseImage->GetHash();
			const auto& imageResult = m_imageHashTable.find(imageHash);
			if (imageResult != m_imageHashTable.cend())
			{
				meshInfo.diffuseImageIndex = imageResult->second;
			}
			else
			{
				m_imageHashTable[imageHash] = m_images.size();
				meshInfo.diffuseImageIndex = m_images.size();
				m_images.emplace_back(diffuseImage);
			}
		}

		if (normalImage.get() != nullptr)
		{
			uint64_t imageHash = normalImage->GetHash();
			const auto& imageResult = m_imageHashTable.find(imageHash);
			if (imageResult != m_imageHashTable.cend())
			{
				meshInfo.normalImageIndex = imageResult->second;
			}
			else
			{
				m_imageHashTable[imageHash] = m_images.size();
				meshInfo.normalImageIndex = m_images.size();
				m_images.emplace_back(normalImage);
			}
		}

		m_active[id] = true;

		return id;
	}

	void SceneManager::Build()
	{
		const std::lock_guard<std::mutex> lock(m_creationMutex);
		m_build = true;
	}

	uint32_t SceneManager::CreateFromOBJ(const std::string& filePath,
		const glm::mat4& transform, const Colour& colour, std::shared_ptr<Image> image)
	{
		if (!std::filesystem::exists(filePath))
		{
			Logger::Error("File at '{}' does not exist.", filePath);
			return 0;
		}

		tinyobj::ObjReader reader;
		if (!reader.ParseFromFile(filePath))
		{
			Logger::Error("Failed to parse OBJ file '{}': {}.", filePath, reader.Error());
			return 0;
		}

		auto& attrib = reader.GetAttrib();
		auto& shapes = reader.GetShapes();
		auto& materials = reader.GetMaterials();

		std::vector<glm::vec3> positions;
		std::vector<glm::vec2> uvs;
		std::vector<Colour> colours;
		std::vector<uint32_t> indices;

		for (const auto& shape : shapes)
		{
			for (const auto& index : shape.mesh.indices)
			{
				indices.push_back(static_cast<uint32_t>(indices.size()));

				positions.emplace_back(glm::vec3(
					attrib.vertices[3 * index.vertex_index + 0],
					attrib.vertices[3 * index.vertex_index + 1],
					attrib.vertices[3 * index.vertex_index + 2]
				));

				colours.emplace_back(Colour());

				uvs.emplace_back(glm::vec2(
					attrib.texcoords[2 * index.texcoord_index + 0],
					1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
				));
			}
		}

		return CreateMesh({ positions, uvs, colours }, indices, transform, colour, image);
	}
}