#include "GLTFLoader.hpp"
#include "Logging/Logger.hpp"
#include "AsyncData.hpp"
#include "Rendering/Resources/GeometryBatch.hpp"
#include "VertexData.hpp"
#include "Image.hpp"
#include "Colour.hpp"
#include <glm/gtx/quaternion.hpp>
#include <filesystem>
#include <chrono>
#include <unordered_map>
#include <stdint.h>
#include <execution>
#include <glm/gtc/type_ptr.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>

using namespace Engine::Logging;
using namespace Engine::Rendering;

namespace Engine
{
	struct ImportState
	{
		const fastgltf::Asset* asset;
		GeometryBatch& geometryBatch;
		std::vector<std::shared_ptr<Image>> loadedImages;
		std::unordered_map<size_t, VertexData> bufferMap;
		std::unordered_map<size_t, std::vector<uint32_t>> indexBufferMap;

		ImportState(const fastgltf::Asset* asset, GeometryBatch& geometryBatch)
			: geometryBatch(geometryBatch)
			, asset(asset)
			, bufferMap()
			, indexBufferMap()
			, loadedImages(asset->images.size())
		{
		}
	};

	template <typename T>
	VertexData CopyBufferData(const fastgltf::Buffer& buffer, const fastgltf::BufferView& bufferView, const fastgltf::Accessor& accessor)
	{
		std::vector<T> data;
		data.resize(accessor.count);

		int32_t byteStride = fastgltf::getComponentBitSize(accessor.componentType);
		const fastgltf::sources::Array* bufferData = std::get_if<fastgltf::sources::Array>(&buffer.data);

		memcpy(data.data(), bufferData->bytes.data() + bufferView.byteOffset + accessor.byteOffset, accessor.count * sizeof(std::decay_t<T>));
		return VertexData(data);
	}

	glm::mat4 GetTransformMatrix(const fastgltf::Node& node, const glm::mat4x4& base)
	{
		if (const auto* pMatrix = std::get_if<fastgltf::Node::TransformMatrix>(&node.transform))
		{
			return base * glm::mat4x4(glm::make_mat4x4(pMatrix->data()));
		}

		else if (const auto* pTransform = std::get_if<fastgltf::TRS>(&node.transform))
		{
			return base
				* glm::translate(glm::mat4(1.0f), glm::make_vec3(pTransform->translation.data()))
				* glm::toMat4(glm::make_quat(pTransform->rotation.data()))
				* glm::scale(glm::mat4(1.0f), glm::make_vec3(pTransform->scale.data()));
		}
		else
		{
			return base;
		}
	}

	template <typename T>
	bool LoadBuffer(ImportState& importState, const fastgltf::Primitive& primitive, std::vector<VertexData>& vertexDataArrays, uint32_t vertexSlot, std::string attributeName)
	{
		size_t accessorIndex = std::numeric_limits<size_t>::max();
		for (const auto& attribute : primitive.attributes)
		{
			if (attribute.first.compare(attributeName) == 0)
			{
				accessorIndex = attribute.second;
				break;
			}
		}

		if (accessorIndex == std::numeric_limits<size_t>::max())
			return false;

		if (vertexSlot >= vertexDataArrays.size())
			vertexDataArrays.resize(vertexSlot + 1);

		const auto& search = importState.bufferMap.find(accessorIndex);
		if (search != importState.bufferMap.cend())
		{
			vertexDataArrays[vertexSlot] = search->second;
			return true;
		}

		const fastgltf::Accessor& accessor = importState.asset->accessors[accessorIndex];
		const fastgltf::BufferView& bufferView = importState.asset->bufferViews[accessor.bufferViewIndex.value()];
		const fastgltf::Buffer& buffer = importState.asset->buffers[bufferView.bufferIndex];

		vertexDataArrays[vertexSlot] = CopyBufferData<T>(buffer, bufferView, accessor);
		importState.bufferMap[accessorIndex] = vertexDataArrays[vertexSlot];
		return true;
	}

	bool LoadMesh(ImportState& importState, size_t meshIndex, const glm::mat4& transform)
	{
		const fastgltf::Asset& asset = *importState.asset;
		const fastgltf::Mesh& mesh = asset.meshes[meshIndex];
		for (auto it = mesh.primitives.begin(); it != mesh.primitives.end(); ++it)
		{
			const fastgltf::Primitive& primitive = *it;

			// We only support indexed geometry.
			if (!primitive.indicesAccessor.has_value())
			{
				return false;
			}

			std::vector<VertexData> vertexDataArrays;
			vertexDataArrays.resize(1);

			// Position
			if (!LoadBuffer<glm::vec3>(importState, primitive, vertexDataArrays, 0, "POSITION"))
				continue;

			// Texture coordinates
			if (!LoadBuffer<glm::vec2>(importState, primitive, vertexDataArrays, 1, "TEXCOORD_0"))
				continue;

			// Normals
			if (!LoadBuffer<glm::vec3>(importState, primitive, vertexDataArrays, 2, "NORMAL"))
				continue;

			std::vector<uint32_t> indices;

			size_t indexAccessorIndex = primitive.indicesAccessor.value();
			const auto& search = importState.indexBufferMap.find(indexAccessorIndex);
			if (search != importState.indexBufferMap.cend())
			{
				indices = search->second;
			}
			else
			{
				const fastgltf::Accessor& indexAccessor = asset.accessors[indexAccessorIndex];
				const fastgltf::BufferView& indexBufferView = asset.bufferViews[indexAccessor.bufferViewIndex.value()];
				const fastgltf::Buffer& indexBuffer = asset.buffers[indexBufferView.bufferIndex];
				int32_t indexByteStride = indexAccessor.componentType == fastgltf::ComponentType::UnsignedShort ? 2 : 4;
				const fastgltf::sources::Array* indexData = std::get_if<fastgltf::sources::Array>(&indexBuffer.data);

				indices.resize(indexAccessor.count);
				if (indexByteStride == 2) // UINT16_T
				{
					for (size_t i = 0; i < indexAccessor.count; ++i)
					{
						const uint16_t* index = reinterpret_cast<const uint16_t*>(static_cast<const uint8_t*>(indexData->bytes.data()) + indexBufferView.byteOffset + indexAccessor.byteOffset + i * indexByteStride);
						indices[i] = static_cast<uint32_t>(*index);
					}
				}
				else
				{
					memcpy(indices.data(), static_cast<const uint8_t*>(indexData->bytes.data()) + indexBufferView.byteOffset + indexAccessor.byteOffset, indexAccessor.count * sizeof(uint32_t));
				}

				importState.indexBufferMap[indexAccessorIndex] = indices;
			}

			Colour colour = {};
			std::shared_ptr<Image> diffuseImage;
			std::shared_ptr<Image> normalImage;
			std::shared_ptr<Image> metallicRoughnessImage;

			if (primitive.materialIndex.has_value())
			{
				const fastgltf::Material& material = asset.materials[primitive.materialIndex.value()];

				// Avoid changing rasterizer culling state, clone the indices and mirror them.
				if (material.doubleSided)
				{
					size_t indexCount = indices.size();
					indices.resize(indexCount * 2);
					for (size_t i = 0; i < indexCount; i += 3)
					{
						indices[indexCount + i] = indices[i + 2];
						indices[indexCount + i + 1] = indices[i + 1];
						indices[indexCount + i + 2] = indices[i];
					}
				}

				const fastgltf::PBRData& pbrData = material.pbrData;
				colour = Colour(pbrData.baseColorFactor[0], pbrData.baseColorFactor[1], pbrData.baseColorFactor[2], pbrData.baseColorFactor[3]);

				if (pbrData.baseColorTexture.has_value())
				{
					const fastgltf::TextureInfo& textureInfo = pbrData.baseColorTexture.value();
					const fastgltf::Texture& texture = asset.textures[textureInfo.textureIndex];
					if (texture.imageIndex.has_value())
					{
						size_t imageIndex = texture.imageIndex.value();
						diffuseImage = importState.loadedImages[imageIndex];
						if (diffuseImage.get() != nullptr)
						{
							assert(!diffuseImage->IsNormalMap());
							assert(!diffuseImage->IsMetallicRoughnessMap());
							assert(diffuseImage->IsSRGB());
						}
					}
				}

				if (pbrData.metallicRoughnessTexture.has_value())
				{
					const fastgltf::TextureInfo& textureInfo = pbrData.metallicRoughnessTexture.value();
					const fastgltf::Texture& texture = asset.textures[textureInfo.textureIndex];
					if (texture.imageIndex.has_value())
					{
						size_t imageIndex = texture.imageIndex.value();
						metallicRoughnessImage = importState.loadedImages[imageIndex];
						if (metallicRoughnessImage.get() != nullptr)
						{
							assert(!metallicRoughnessImage->IsNormalMap());
							assert(metallicRoughnessImage->IsMetallicRoughnessMap());
							assert(!metallicRoughnessImage->IsSRGB());
						}
					}
				}

				if (material.normalTexture.has_value())
				{
					const fastgltf::TextureInfo& textureInfo = material.normalTexture.value();
					const fastgltf::Texture& texture = asset.textures[textureInfo.textureIndex];
					if (texture.imageIndex.has_value())
					{
						size_t imageIndex = texture.imageIndex.value();
						normalImage = importState.loadedImages[imageIndex];
						if (normalImage.get() != nullptr)
						{
							assert(normalImage->IsNormalMap());
							assert(!normalImage->IsMetallicRoughnessMap());
							assert(!normalImage->IsSRGB());
						}
					}
				}
			}

			importState.geometryBatch.CreateMesh(vertexDataArrays, indices, transform, colour, diffuseImage, normalImage, metallicRoughnessImage, true);
		}

		return true;
	}

	bool LoadNode(ImportState& importState, size_t nodeIndex, glm::mat4 transform)
	{
		auto& node = importState.asset->nodes[nodeIndex];
		transform = GetTransformMatrix(node, transform);

		if (node.meshIndex.has_value())
		{
			if (!LoadMesh(importState, node.meshIndex.value(), transform))
			{
				return false;
			}
		}

		for (auto& child : node.children)
		{
			if (!LoadNode(importState, child, transform))
			{
				return false;
			}
		}

		return true;
	}

	bool LoadData(ImportState& importState)
	{
		glm::mat4 transform = glm::mat4(1.0f);

		const fastgltf::Scene& scene = importState.asset->scenes[importState.asset->defaultScene.value()];
		for (auto it = scene.nodeIndices.begin(); it != scene.nodeIndices.end(); ++it)
		{
			if (!LoadNode(importState, *it, transform))
			{
				return false;
			}
		}

		return true;
	}

	bool GLTFLoader::LoadGLTF(const std::filesystem::path& filePath, GeometryBatch& geometryBatch, AsyncData* asyncData)
	{
		if (!std::filesystem::exists(filePath))
		{
			Logger::Error("File at '{}' does not exist.", filePath.string());
			return false;
		}

		auto parseStartTime = std::chrono::high_resolution_clock::now();

		constexpr auto gltfOptions =
			fastgltf::Options::DontRequireValidAssetMember |
			fastgltf::Options::LoadGLBBuffers |
			fastgltf::Options::LoadExternalBuffers |
			fastgltf::Options::LoadExternalImages;

		auto path = std::filesystem::path(filePath);

		fastgltf::GltfDataBuffer data;
		data.loadFromFile(path);

		fastgltf::Parser parser(fastgltf::Extensions::KHR_lights_punctual);
		fastgltf::Asset asset;

		auto type = fastgltf::determineGltfFileType(&data);
		if (type == fastgltf::GltfType::glTF)
		{
			fastgltf::Expected<fastgltf::Asset> parserResult = parser.loadGltfJson(&data, path.parent_path(), gltfOptions);

			if (parserResult.error() != fastgltf::Error::None)
			{
				Logger::Error("Failed to parse GLTF file '{}'.", fastgltf::to_underlying(parserResult.error()));
				return false;
			}

			asset = std::move(parserResult.get());
		}
		else if (type == fastgltf::GltfType::GLB)
		{
			fastgltf::Expected<fastgltf::Asset> parserResult = parser.loadGltfBinary(&data, path.parent_path(), gltfOptions);

			if (parserResult.error() != fastgltf::Error::None)
			{
				Logger::Error("Failed to parse GLTF file '{}'.", fastgltf::to_underlying(parserResult.error()));
				return false;
			}

			asset = std::move(parserResult.get());
		}
		else
		{
			Logger::Error("Failed to determine GLTF container type.");
			return false;
		}

		auto parseEndTime = std::chrono::high_resolution_clock::now();
		float parseDeltaTime = std::chrono::duration<float, std::chrono::seconds::period>(parseEndTime - parseStartTime).count();
		Logger::Verbose("GLTF file parsed in {} seconds.", parseDeltaTime);

		if (asyncData != nullptr)
			asyncData->AddSubProgress(100.0f);

		auto loadStartTime = std::chrono::high_resolution_clock::now();

		ImportState importState(&asset, geometryBatch);

		// Track if images should be treated as SRGB, normal maps, etc.
		std::vector<ImageFlags> m_imageFlags;
		m_imageFlags.resize(asset.images.size());
		for (const fastgltf::Material& material : asset.materials)
		{
			const fastgltf::PBRData& pbrData = material.pbrData;
			if (pbrData.baseColorTexture.has_value())
			{
				const fastgltf::TextureInfo& textureInfo = pbrData.baseColorTexture.value();
				const fastgltf::Texture& texture = asset.textures[textureInfo.textureIndex];
				if (texture.imageIndex.has_value())
				{
					size_t imageIndex = texture.imageIndex.value();
					if (imageIndex < asset.images.size())
					{
						m_imageFlags[imageIndex] |= ImageFlags::SRGB;
					}
				}
			}

			if (pbrData.metallicRoughnessTexture.has_value())
			{
				const fastgltf::TextureInfo& textureInfo = pbrData.metallicRoughnessTexture.value();
				const fastgltf::Texture& texture = asset.textures[textureInfo.textureIndex];
				if (texture.imageIndex.has_value())
				{
					size_t imageIndex = texture.imageIndex.value();
					if (imageIndex < asset.images.size())
					{
						m_imageFlags[imageIndex] |= ImageFlags::MetallicRoughnessMap;
					}
				}
			}

			if (material.normalTexture.has_value())
			{
				const fastgltf::TextureInfo& textureInfo = material.normalTexture.value();
				const fastgltf::Texture& texture = asset.textures[textureInfo.textureIndex];
				if (texture.imageIndex.has_value())
				{
					size_t imageIndex = texture.imageIndex.value();
					if (imageIndex < asset.images.size())
					{
						m_imageFlags[imageIndex] |= ImageFlags::NormalMap;
					}
				}
			}
		}

		float subTicks = 200.0f / static_cast<float>(asset.textures.size());
		std::vector<std::atomic_bool> imageWriteStates(asset.images.size());

		std::for_each(
			std::execution::par,
			asset.textures.cbegin(),
			asset.textures.cend(),
			[&asset, &importState, &m_imageFlags, &imageWriteStates, asyncData, subTicks](const fastgltf::Texture& texture)
			{
				if (texture.imageIndex.has_value())
				{
					size_t imageIndex = texture.imageIndex.value();
					if (!imageWriteStates[imageIndex].exchange(true))
					{
						const fastgltf::Image& gltfImage = asset.images[imageIndex];
						const fastgltf::sources::BufferView* bufferViewInfo = std::get_if<fastgltf::sources::BufferView>(&gltfImage.data);
						if (bufferViewInfo != nullptr)
						{
							const fastgltf::BufferView& imageBufferView = asset.bufferViews[bufferViewInfo->bufferViewIndex];
							const fastgltf::Buffer& imageBuffer = asset.buffers[imageBufferView.bufferIndex];
							const fastgltf::sources::Array* imageData = std::get_if<fastgltf::sources::Array>(&imageBuffer.data);

							importState.loadedImages[imageIndex] = std::make_shared<Image>();

							if (!importState.loadedImages[imageIndex]->LoadFromMemory(imageData->bytes.data() + imageBufferView.byteOffset, imageBufferView.byteLength, m_imageFlags[imageIndex]))
							{
								importState.loadedImages[imageIndex].reset();
								Logger::Error("Failed to load image at index {}.", imageIndex);
							}
						}
					}
				}

				if (asyncData != nullptr)
					asyncData->AddSubProgress(subTicks);
			});


		bool result = LoadData(importState);

		if (asyncData != nullptr)
			asyncData->AddSubProgress(100.0f);

		auto loadEndTime = std::chrono::high_resolution_clock::now();
		float loadDeltaTime = std::chrono::duration<float, std::chrono::seconds::period>(loadEndTime - loadStartTime).count();
		Logger::Verbose("GLTF file loaded in {} seconds.", loadDeltaTime);

		return true;
	}
}