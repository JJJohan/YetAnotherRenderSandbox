#include "MeshManager.hpp"
#include "Shader.hpp"
#include "Core/Logging/Logger.hpp"
#include <filesystem>
#include "GltfLoader.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

using namespace Engine::Logging;

namespace Engine::Rendering
{
	MeshManager::MeshManager()
		:m_recycledIds()
		, m_active()
		, m_creationMutex()
		, m_updateFlags()
		, m_vertexUpdateFlags()
		, m_meshCapacity()
		, m_shaders()
		, m_vertexDataArrays()
		, m_indexArrays()
		, m_colours()
		, m_transforms()
		, m_images()
	{
	}

	uint32_t MeshManager::CreateMesh(const Shader* shader,
		const std::vector<VertexData>& vertexData,
		const std::vector<uint32_t>& indices,
		const glm::mat4& transform,
		const Colour& colour,
		std::shared_ptr<Image> image)
	{
		const std::lock_guard<std::mutex> lock(m_creationMutex);

		uint32_t id;
		if (!m_recycledIds.empty())
		{
			id = m_recycledIds.top();
			m_recycledIds.pop();
		}
		else
		{
			IncrementSize();
			id = m_meshCapacity++;
		}

		m_shaders[id] = shader;
		m_vertexDataArrays[id] = vertexData;
		m_indexArrays[id] = indices;
		m_colours[id] = colour;
		m_transforms[id] = transform;
		m_images[id] = image;
		m_updateFlags[id] = MeshUpdateFlagBits::None; // Creation will update everything.
		m_vertexUpdateFlags[id] = 0;

		m_active[id] = true;

		return id;
	}

	uint32_t MeshManager::CreateFromOBJ(const Shader* shader, const std::string& filePath,
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

		return CreateMesh(shader, { positions, uvs, colours }, indices, transform, colour, image);
	}

	void MeshManager::DestroyMesh(uint32_t id)
	{
		m_active[id] = false;

		m_shaders[id] = nullptr;
		m_vertexDataArrays[id] = {};
		m_indexArrays[id] = {};
		m_images[id].reset();
		m_updateFlags[id] = MeshUpdateFlagBits::None;
		m_vertexUpdateFlags[id] = 0;

		m_recycledIds.push(id);
	}

	void MeshManager::IncrementSize()
	{
		m_active.push_back(false);

		m_shaders.push_back(nullptr);
		m_vertexDataArrays.push_back({});
		m_indexArrays.push_back({});
		m_colours.push_back({});
		m_transforms.push_back({});
		m_images.push_back(nullptr);
		m_updateFlags.push_back(MeshUpdateFlagBits::None);
		m_vertexUpdateFlags.push_back(0);
	}

	void MeshManager::SetVertexData(uint32_t id, uint32_t slot, const VertexData& data)
	{
		std::vector<VertexData>& vertexBuffers = m_vertexDataArrays[id];
		if (slot >= vertexBuffers.size())
		{
			Logger::Error("Index out of range.");
			return;
		}

		vertexBuffers[slot] = data;
		m_updateFlags[id] = m_updateFlags[id] | MeshUpdateFlagBits::VertexData;
		m_vertexUpdateFlags[id] |= 1 << slot;
	}

	const VertexData& MeshManager::GetVertexData(uint32_t id, uint32_t slot) const
	{
		const std::vector<VertexData>& vertexBuffers = m_vertexDataArrays[id];
		if (slot >= vertexBuffers.size())
		{
			Logger::Error("Index out of range.");
			throw new std::out_of_range("Index out of range.");
		}

		return vertexBuffers[id];
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
		m_updateFlags[id] = m_updateFlags[id] | MeshUpdateFlagBits::Uniforms;
	}

	const Colour& MeshManager::GetColour(uint32_t id) const
	{
		return m_colours[id];
	}

	void MeshManager::SetTransform(uint32_t id, const glm::mat4& transform)
	{
		m_transforms[id] = transform;
		m_updateFlags[id] = m_updateFlags[id] | MeshUpdateFlagBits::Uniforms;
	}

	const glm::mat4& MeshManager::GetTransform(uint32_t id) const
	{
		return m_transforms[id];
	}

	void MeshManager::SetImage(uint32_t id, std::shared_ptr<Image> image)
	{
		m_images[id] = image;
		m_updateFlags[id] = m_updateFlags[id] | MeshUpdateFlagBits::Image;
	}
}