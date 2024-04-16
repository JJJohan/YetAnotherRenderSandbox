#include "GeometryBatch.hpp"
#include "../IPhysicalDevice.hpp"
#include "../IDevice.hpp"
#include "../Resources/IBuffer.hpp"
#include "../Resources/IRenderImage.hpp"
#include "../Resources/IImageSampler.hpp"
#include "../IResourceFactory.hpp"
#include "../Resources/ICommandBuffer.hpp"
#include "../Resources/MeshInfo.hpp"
#include "Core/MeshOptimiser.hpp"
#include "Core/Image.hpp"
#include "../Renderer.hpp"
#include "../Resources/RenderMeshInfo.hpp"
#include "Core/Logger.hpp"
#include "Core/ChunkData.hpp"
#include "Core/AsyncData.hpp"
#include "Core/VertexData.hpp"
#include "Core/Colour.hpp"
#include <execution>

namespace Engine::Rendering
{
	GeometryBatch::GeometryBatch(Renderer& renderer)
		: m_renderer(renderer)
		, m_indirectDrawBuffer(nullptr)
		, m_vertexBuffers()
		, m_indexBuffer(nullptr)
		, m_boundsBuffer(nullptr)
		, m_meshInfoBuffer(nullptr)
		, m_imageArray()
		, m_vertexOffsets()
		, m_indexOffsets()
		, m_indexCounts()
		, m_recycledIds()
		, m_active()
		, m_creating(true)
		, m_meshCapacity(0)
		, m_vertexDataArrays()
		, m_indexArrays()
		, m_meshInfos()
		, m_images()
		, m_vertexBufferOffsets()
		, m_indexBufferOffsets()
		, m_imageHashTable()
		, m_vertexDataHashTable()
		, m_indexDataHashTable()
		, m_indirectDrawCommands()
	{
	}

	bool GeometryBatch::CreateMesh(const std::vector<VertexData>& vertexData,
		const std::vector<uint32_t>& indices,
		const glm::mat4& transform,
		const Colour& colour,
		std::shared_ptr<Image> diffuseImage,
		std::shared_ptr<Image> normalImage,
		std::shared_ptr<Image> metallicRoughnessImage,
		bool convertToLHS)
	{
		if (vertexData.empty())
		{
			Logger::Error("Empty vertex buffer vector not permitted.");
			return false;
		}

		uint32_t id;
		if (!m_recycledIds.empty())
		{
			id = m_recycledIds.top();
			m_recycledIds.pop();
			m_active[id] = true;
		}
		else
		{
			m_meshInfos.push_back({});
			m_active.push_back(true);
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
			for (size_t i = 0; i < vertexData.size(); ++i)
			{
				if (i == 0 && convertToLHS)
				{
					uint32_t vertexCount = vertexData[i].GetCount();
					const glm::vec3* vertices = vertexData[i].GetData<glm::vec3>();
					std::vector<glm::vec3> newPositions(vertexCount);
					for (uint32_t j = 0; j < vertexCount; ++j)
					{
						const glm::vec3& v = vertices[j];
						newPositions[j] = glm::vec3(-v.x, v.y, v.z);
					}

					localVertexData.emplace_back(std::make_unique<VertexData>(newPositions));
				}
				else
				{
					localVertexData.emplace_back(std::make_unique<VertexData>(vertexData[i]));
				}
			}

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

		if (metallicRoughnessImage.get() != nullptr)
		{
			uint64_t imageHash = metallicRoughnessImage->GetHash();
			const auto& imageResult = m_imageHashTable.find(imageHash);
			if (imageResult != m_imageHashTable.cend())
			{
				meshInfo.metallicRoughnessImageIndex = imageResult->second;
			}
			else
			{
				m_imageHashTable[imageHash] = m_images.size();
				meshInfo.metallicRoughnessImageIndex = m_images.size();
				m_images.emplace_back(metallicRoughnessImage);
			}
		}

		return true;
	}

	bool GeometryBatch::Optimise()
	{
		std::vector<uint32_t>& indices = *m_indexArrays.back();
		std::vector<std::unique_ptr<VertexData>>& vertexDataArray = m_vertexDataArrays.back();
		return MeshOptimiser::Optimise(indices, vertexDataArray);
	}

	bool GeometryBatch::UploadIndirectDrawBuffer(const IDevice& device, const ICommandBuffer& commandBuffer, const IResourceFactory& resourceFactory,
		std::vector<std::unique_ptr<IBuffer>>& temporaryBuffers, IBuffer* buffer, const void* data, uint32_t drawCount, size_t dataSize)
	{
		// Draw count is stored in the first 4 bytes.
		size_t totalSize = sizeof(uint32_t) + dataSize;

		bool initialised = buffer->Initialise("indirectBuffer", device, totalSize,
			BufferUsageFlags::TransferDst | BufferUsageFlags::IndirectBuffer | BufferUsageFlags::StorageBuffer,
			MemoryUsage::AutoPreferDevice,
			AllocationCreateFlags::None,
			SharingMode::Exclusive);

		if (!initialised)
		{
			return false;
		}

		IBuffer* stagingBuffer = temporaryBuffers.emplace_back(std::move(resourceFactory.CreateBuffer())).get();
		if (!stagingBuffer->Initialise("indirectStagingBuffer", device, totalSize,
			BufferUsageFlags::TransferSrc, MemoryUsage::Auto,
			AllocationCreateFlags::HostAccessSequentialWrite | AllocationCreateFlags::Mapped,
			SharingMode::Exclusive))
		{
			return false;
		}

		// Upload draw count.
		if (!stagingBuffer->UpdateContents(&drawCount, 0, sizeof(uint32_t)))
			return false;

		// Upload actual indirect commands.
		if (!stagingBuffer->UpdateContents(data, sizeof(uint32_t), dataSize))
			return false;

		stagingBuffer->Copy(commandBuffer, *buffer, totalSize);

		commandBuffer.MemoryBarrier(MaterialStageFlags::Transfer, MaterialAccessFlags::MemoryWrite,
			MaterialStageFlags::DrawIndirect, MaterialAccessFlags::IndirectCommandRead);

		return true;
	}

	bool GeometryBatch::SetupIndirectDrawBuffer(const IDevice& device, const ICommandBuffer& commandBuffer,
		ChunkData* chunkData, std::vector<std::unique_ptr<IBuffer>>& temporaryBuffers, const IResourceFactory& resourceFactory)
	{
		if (chunkData != nullptr && chunkData->LoadedFromDisk())
		{
			ChunkMemoryEntry entry;
			if (!chunkData->GetGenericData(static_cast<uint32_t>(CachedDataType::IndirectDrawBuffer), entry))
			{
				return false;
			}

			std::vector<uint8_t> decompressBuffer;
			chunkData->Decompress(entry, decompressBuffer);

			m_indirectDrawBuffer = std::move(resourceFactory.CreateBuffer());

			m_meshCapacity = static_cast<uint32_t>(decompressBuffer.size() / sizeof(IndexedIndirectCommand));

			return UploadIndirectDrawBuffer(device, commandBuffer, resourceFactory, temporaryBuffers, m_indirectDrawBuffer.get(),
				decompressBuffer.data(), m_meshCapacity, decompressBuffer.size());
		}

		std::vector<IndexedIndirectCommand> indirectBufferData;
		indirectBufferData.reserve(m_meshCapacity);

		for (size_t i = 0; i < m_meshCapacity; ++i)
		{
			if (!m_active[i])
				continue;

			const MeshInfo& meshInfo = m_meshInfos[i];
			IndexedIndirectCommand indirectCommand{};
			indirectCommand.VertexOffset = m_vertexOffsets[meshInfo.vertexBufferIndex];
			indirectCommand.FirstIndex = m_indexOffsets[meshInfo.indexBufferIndex];
			indirectCommand.IndexCount = m_indexCounts[meshInfo.indexBufferIndex];
			indirectCommand.InstanceCount = 1;
			indirectBufferData.emplace_back(indirectCommand);
		}

		m_indirectDrawBuffer = std::move(resourceFactory.CreateBuffer());

		size_t totalSize = indirectBufferData.size() * sizeof(IndexedIndirectCommand);

		if (!UploadIndirectDrawBuffer(device, commandBuffer, resourceFactory, temporaryBuffers, m_indirectDrawBuffer.get(),
			indirectBufferData.data(), m_meshCapacity, totalSize))
			return false;

		if (chunkData)
		{
			std::span<uint8_t> cacheData(reinterpret_cast<uint8_t*>(indirectBufferData.data()), totalSize);
			chunkData->SetGenericData(static_cast<uint32_t>(CachedDataType::IndirectDrawBuffer), cacheData);
		}

		return true;
	}

	bool GeometryBatch::SetupVertexBuffers(const IDevice& device, const ICommandBuffer& commandBuffer,
		ChunkData* chunkData, std::vector<std::unique_ptr<IBuffer>>& temporaryBuffers, const IResourceFactory& resourceFactory)
	{
		if (chunkData != nullptr && chunkData->LoadedFromDisk())
		{
			std::array<ChunkMemoryEntry, 3> cacheEntries;
			if (!chunkData->GetVertexData(VertexBufferType::Positions, cacheEntries[0]))
				return false;
			if (!chunkData->GetVertexData(VertexBufferType::TextureCoordinates, cacheEntries[1]))
				return false;
			if (!chunkData->GetVertexData(VertexBufferType::Normals, cacheEntries[2]))
				return false;

			std::vector<uint8_t> decompressBuffer;
			m_vertexBuffers.resize(cacheEntries.size());
			for (size_t i = 0; i < cacheEntries.size(); ++i)
			{
				m_vertexBuffers[i] = std::move(resourceFactory.CreateBuffer());
				IBuffer* buffer = m_vertexBuffers[i].get();

				chunkData->Decompress(cacheEntries[i], decompressBuffer);
				const std::span<const uint8_t> span(decompressBuffer.begin(), cacheEntries[i].UncompressedSize);

				bool initialised = buffer->Initialise("vertexBuffer", device, span.size(),
					BufferUsageFlags::TransferDst | BufferUsageFlags::VertexBuffer,
					MemoryUsage::AutoPreferDevice,
					AllocationCreateFlags::None,
					SharingMode::Exclusive);

				if (!initialised)
				{
					return false;
				}

				if (!CreateStagingBuffer(device, resourceFactory, commandBuffer, buffer, span.data(),
					span.size(), temporaryBuffers))
					return false;
			}

			commandBuffer.MemoryBarrier(MaterialStageFlags::Transfer, MaterialAccessFlags::MemoryWrite,
				MaterialStageFlags::VertexInput, MaterialAccessFlags::VertexAttributeRead);

			return true;
		}

		if (m_vertexDataArrays.empty())
			return false;

		m_vertexBuffers.resize(m_vertexDataArrays[0].size());
		m_vertexOffsets.resize(m_vertexDataArrays.size());

		for (size_t vertexBit = 0; vertexBit < m_vertexBuffers.size(); ++vertexBit)
		{
			uint64_t totalSize = 0;
			for (size_t i = 0; i < m_vertexDataArrays.size(); ++i)
			{
				if (!m_active[i])
					continue;

				const std::unique_ptr<VertexData>& data = m_vertexDataArrays[i][vertexBit];
				totalSize += static_cast<uint64_t>(data->GetElementSize()) * data->GetCount();
			}

			std::vector<uint8_t> vertexBufferData;
			vertexBufferData.resize(totalSize);
			totalSize = 0;
			size_t vertexOffset = 0;
			for (size_t i = 0; i < m_vertexDataArrays.size(); ++i)
			{
				if (!m_active[i])
					continue;

				const std::unique_ptr<VertexData>& data = m_vertexDataArrays[i][vertexBit];
				uint32_t size = data->GetElementSize() * data->GetCount();
				memcpy(vertexBufferData.data() + totalSize, data->GetData<uint8_t>(), size);

				m_vertexOffsets[i] = static_cast<uint32_t>(vertexOffset);

				vertexOffset += data->GetCount();
				totalSize += size;
			}

			// TODO: Clean up!
			if (chunkData)
			{
				if (vertexBit == 0)
					chunkData->SetVertexData(VertexBufferType::Positions, vertexBufferData);
				else if (vertexBit == 1)
					chunkData->SetVertexData(VertexBufferType::TextureCoordinates, vertexBufferData);
				else if (vertexBit == 2)
					chunkData->SetVertexData(VertexBufferType::Normals, vertexBufferData);
				else
					throw;
			}

			m_vertexBuffers[vertexBit] = std::move(resourceFactory.CreateBuffer());
			IBuffer* buffer = m_vertexBuffers[vertexBit].get();

			bool initialised = buffer->Initialise("vertexBuffer", device, totalSize,
				BufferUsageFlags::TransferDst | BufferUsageFlags::VertexBuffer,
				MemoryUsage::AutoPreferDevice,
				AllocationCreateFlags::None,
				SharingMode::Exclusive);

			if (!initialised)
			{
				return false;
			}

			if (!CreateStagingBuffer(device, resourceFactory, commandBuffer, buffer, vertexBufferData.data(),
				totalSize, temporaryBuffers))
				return false;
		}

		commandBuffer.MemoryBarrier(MaterialStageFlags::Transfer, MaterialAccessFlags::MemoryWrite,
			MaterialStageFlags::VertexInput, MaterialAccessFlags::VertexAttributeRead);

		return true;
	}

	bool GeometryBatch::UploadBoundsBuffer(const IDevice& device, const ICommandBuffer& commandBuffer, const IResourceFactory& resourceFactory,
		std::vector<std::unique_ptr<IBuffer>>& temporaryBuffers, IBuffer* buffer, const void* data, size_t dataSize)
	{
		bool initialised = buffer->Initialise("boundsBuffer", device, dataSize,
			BufferUsageFlags::TransferDst | BufferUsageFlags::StorageBuffer,
			MemoryUsage::AutoPreferDevice,
			AllocationCreateFlags::None,
			SharingMode::Exclusive);

		if (!initialised)
		{
			return false;
		}

		if (!CreateStagingBuffer(device, resourceFactory, commandBuffer, buffer, data,
			dataSize, temporaryBuffers))
			return false;

		commandBuffer.MemoryBarrier(MaterialStageFlags::Transfer, MaterialAccessFlags::MemoryWrite,
			MaterialStageFlags::ComputeShader, MaterialAccessFlags::ShaderRead);

		return true;
	}

	bool GeometryBatch::SetupBoundsBuffer(const IDevice& device, const ICommandBuffer& commandBuffer,
		ChunkData* chunkData, std::vector<std::unique_ptr<IBuffer>>& temporaryBuffers, const IResourceFactory& resourceFactory)
	{
		if (chunkData != nullptr && chunkData->LoadedFromDisk())
		{
			ChunkMemoryEntry entry;
			if (!chunkData->GetGenericData(static_cast<uint32_t>(CachedDataType::BoundsBuffer), entry))
			{
				return false;
			}

			std::vector<uint8_t> decompressBuffer;
			chunkData->Decompress(entry, decompressBuffer);

			m_boundsBuffer = std::move(resourceFactory.CreateBuffer());

			return UploadBoundsBuffer(device, commandBuffer, resourceFactory, temporaryBuffers,
				m_boundsBuffer.get(), decompressBuffer.data(), decompressBuffer.size());
		}

		std::vector<glm::vec4> boundsData;
		boundsData.reserve(m_meshCapacity);

		for (size_t i = 0; i < m_vertexDataArrays.size(); ++i)
		{
			if (!m_active[i])
				continue;

			const std::unique_ptr<VertexData>& data = m_vertexDataArrays[i][0];
			uint32_t size = data->GetCount();
			const auto& positionData = data->GetData<glm::vec3>();

			float radius = 0.0f;
			glm::vec3 center{};
			for (uint32_t j = 0; j < size; ++j)
			{
				center += positionData[j];
			}
			center /= static_cast<float>(size);
			radius = glm::distance2(positionData[0], center);
			for (uint32_t j = 1; j < size; ++j)
			{
				radius = std::max(radius, glm::distance2(positionData[j], center));
			}
			radius = std::nextafter(sqrtf(radius), std::numeric_limits<float>::max());

			const MeshInfo& meshInfo = m_meshInfos[i];
			center += glm::vec3(meshInfo.transform[3]);

			boundsData.emplace_back(glm::vec4(center, radius));
		}

		m_boundsBuffer = std::move(resourceFactory.CreateBuffer());
		size_t totalSize = boundsData.size() * sizeof(glm::vec4);

		if (!UploadBoundsBuffer(device, commandBuffer, resourceFactory, temporaryBuffers,
			m_boundsBuffer.get(), boundsData.data(), totalSize))
			return false;

		if (chunkData)
		{
			std::span<uint8_t> cacheData(reinterpret_cast<uint8_t*>(boundsData.data()), totalSize);
			chunkData->SetGenericData(static_cast<uint32_t>(CachedDataType::BoundsBuffer), cacheData);
		}

		return true;
	}

	bool GeometryBatch::UploadIndexBuffer(const IDevice& device, const ICommandBuffer& commandBuffer, const IResourceFactory& resourceFactory,
		std::vector<std::unique_ptr<IBuffer>>& temporaryBuffers, IBuffer* buffer, const void* data, size_t dataSize)
	{
		bool initialised = buffer->Initialise("indexBuffer", device, dataSize,
			BufferUsageFlags::TransferDst | BufferUsageFlags::IndexBuffer,
			MemoryUsage::AutoPreferDevice,
			AllocationCreateFlags::None,
			SharingMode::Exclusive);

		if (!initialised)
		{
			return false;
		}

		if (!CreateStagingBuffer(device, resourceFactory, commandBuffer, buffer, data,
			dataSize, temporaryBuffers))
			return false;

		commandBuffer.MemoryBarrier(MaterialStageFlags::Transfer, MaterialAccessFlags::MemoryWrite,
			MaterialStageFlags::VertexInput, MaterialAccessFlags::IndexRead);

		return true;
	}

	bool GeometryBatch::SetupIndexBuffer(const IDevice& device, const ICommandBuffer& commandBuffer,
		ChunkData* chunkData, std::vector<std::unique_ptr<IBuffer>>& temporaryBuffers, const IResourceFactory& resourceFactory)
	{
		if (chunkData != nullptr && chunkData->LoadedFromDisk())
		{
			ChunkMemoryEntry entry;
			if (!chunkData->GetGenericData(static_cast<uint32_t>(CachedDataType::IndexBuffer), entry))
			{
				return false;
			}

			std::vector<uint8_t> decompressBuffer;
			chunkData->Decompress(entry, decompressBuffer);

			m_indexBuffer = std::move(resourceFactory.CreateBuffer());

			return UploadIndexBuffer(device, commandBuffer, resourceFactory, temporaryBuffers,
				m_indexBuffer.get(), decompressBuffer.data(), decompressBuffer.size());
		}

		m_indexOffsets.resize(m_indexArrays.size());
		m_indexCounts.resize(m_indexArrays.size());

		uint64_t totalSize = 0;
		for (size_t i = 0; i < m_indexArrays.size(); ++i)
		{
			if (!m_active[i])
				continue;

			const std::unique_ptr<std::vector<uint32_t>>& data = m_indexArrays[i];
			totalSize += data->size() * sizeof(uint32_t);
		}

		std::vector<uint8_t> indexBufferData;
		indexBufferData.resize(totalSize);
		totalSize = 0;
		size_t index = 0;
		size_t indexOffset = 0;
		for (size_t i = 0; i < m_indexArrays.size(); ++i)
		{
			if (!m_active[i])
				continue;

			const std::unique_ptr<std::vector<uint32_t>>& data = m_indexArrays[i];
			uint32_t size = static_cast<uint32_t>(data->size()) * sizeof(uint32_t);
			memcpy(indexBufferData.data() + totalSize, data->data(), size);
			m_indexOffsets[i] = static_cast<uint32_t>(indexOffset);
			m_indexCounts[i] = static_cast<uint32_t>(data->size());
			indexOffset += data->size();
			totalSize += size;
		}


		m_indexBuffer = std::move(resourceFactory.CreateBuffer());

		if (!UploadIndexBuffer(device, commandBuffer, resourceFactory, temporaryBuffers,
			m_indexBuffer.get(), indexBufferData.data(), totalSize))
			return false;

		if (chunkData)
			chunkData->SetGenericData(static_cast<uint32_t>(CachedDataType::IndexBuffer), indexBufferData);

		return true;
	}

	bool GeometryBatch::CreateStagingBuffer(const IDevice& device, const IResourceFactory& resourceFactory,
		const ICommandBuffer& commandBuffer, const IBuffer* destinationBuffer, const void* data, uint64_t size,
		std::vector<std::unique_ptr<IBuffer>>& copyBufferCollection)
	{
		IBuffer* stagingBuffer = copyBufferCollection.emplace_back(std::move(resourceFactory.CreateBuffer())).get();
		if (!stagingBuffer->Initialise("stagingBuffer", device, size,
			BufferUsageFlags::TransferSrc, MemoryUsage::Auto,
			AllocationCreateFlags::HostAccessSequentialWrite | AllocationCreateFlags::Mapped,
			SharingMode::Exclusive))
		{
			return false;
		}

		if (!stagingBuffer->UpdateContents(data, 0, size))
			return false;

		stagingBuffer->Copy(commandBuffer, *destinationBuffer, size);

		return true;
	}

	bool GeometryBatch::CreateImageStagingBuffer(const IDevice& device, const IResourceFactory& resourceFactory,
		const ICommandBuffer& commandBuffer, const IRenderImage* destinationImage, uint32_t mipLevel, const void* data, uint64_t size,
		std::vector<std::unique_ptr<IBuffer>>& copyBufferCollection)
	{
		IBuffer* stagingBuffer = copyBufferCollection.emplace_back(std::move(resourceFactory.CreateBuffer())).get();
		if (!stagingBuffer->Initialise("imageStagingBuffer", device, size,
			BufferUsageFlags::TransferSrc, MemoryUsage::Auto,
			AllocationCreateFlags::HostAccessSequentialWrite | AllocationCreateFlags::Mapped,
			SharingMode::Exclusive))
		{
			return false;
		}

		if (!stagingBuffer->UpdateContents(data, 0, size))
			return false;

		stagingBuffer->CopyToImage(mipLevel, commandBuffer, *destinationImage);

		return true;
	}

	bool GeometryBatch::SetupRenderImage(AsyncData* asyncData, const IDevice& device, const IPhysicalDevice& physicalDevice,
		const ICommandBuffer& commandBuffer, ChunkData* chunkData, std::vector<std::unique_ptr<IBuffer>>& temporaryBuffers,
		const IResourceFactory& resourceFactory, float maxAnisotropy, uint32_t& imageCount)
	{
		if (chunkData != nullptr && chunkData->LoadedFromDisk())
		{
			std::vector<ImageData>* cachedImageData;
			if (!chunkData->GetImageData(&cachedImageData))
			{
				return false;
			}

			imageCount = static_cast<uint32_t>(cachedImageData->size());
			m_imageArray.reserve(cachedImageData->size());

			float subTicks = 400.0f / static_cast<float>(cachedImageData->size());

			std::vector<uint8_t> decompressBuffer;
			for (auto it = cachedImageData->begin(); it != cachedImageData->end(); ++it)
			{
				const ImageData& imageData = *it;
				Format format = static_cast<Format>(imageData.Header.Format);
				glm::uvec3 dimensions(imageData.Header.Width, imageData.Header.Height, 1);

				chunkData->Decompress(imageData.Entry, decompressBuffer);

				std::vector<std::span<const uint8_t>> spans(imageData.Header.MipLevels);
				uint64_t offset = 0;
				uint64_t size = imageData.Header.FirstMipSize;
				for (uint32_t i = 0; i < imageData.Header.MipLevels; ++i)
				{
					spans[i] = std::span<const uint8_t>(decompressBuffer.begin() + offset, size);
					offset += size;
					size /= 4;
				}

				std::unique_ptr<IRenderImage>& renderImage = m_imageArray.emplace_back(std::move(resourceFactory.CreateRenderImage()));
				bool imageInitialised = renderImage->Initialise("SceneImage", device, ImageType::e2D, format, dimensions,
					imageData.Header.MipLevels, 1, ImageTiling::Optimal,
					ImageUsageFlags::TransferSrc | ImageUsageFlags::TransferDst | ImageUsageFlags::Sampled, ImageAspectFlags::Color,
					MemoryUsage::AutoPreferDevice, AllocationCreateFlags::None, SharingMode::Exclusive);

				if (!imageInitialised)
				{
					return false;
				}

				renderImage->TransitionImageLayout(device, commandBuffer, ImageLayout::TransferDst);

				for (uint32_t i = 0; i < imageData.Header.MipLevels; ++i)
				{
					if (!CreateImageStagingBuffer(device, resourceFactory, commandBuffer, renderImage.get(), i, spans[i].data(),
						spans[i].size(), temporaryBuffers))
						return false;
				}

				renderImage->TransitionImageLayout(device, commandBuffer, ImageLayout::ShaderReadOnly);

				if (asyncData != nullptr)
					asyncData->AddSubProgress(subTicks);
			}

			commandBuffer.MemoryBarrier(MaterialStageFlags::Transfer, MaterialAccessFlags::MemoryWrite,
				MaterialStageFlags::FragmentShader, MaterialAccessFlags::ShaderRead);

			return true;
		}

		m_imageArray.reserve(m_images.size());
		imageCount = 0;

		bool compress = false;
		if (physicalDevice.SupportsBCTextureCompression() && physicalDevice.FormatSupported(Format::Bc7SrgbBlock))
		{
			compress = true;
		}

		if (asyncData != nullptr)
			asyncData->InitSubProgress("Optimising Images", 400.0f);
		float imageSubTicks = 400.0f / static_cast<float>(m_images.size());

		uint32_t totalImageCount = static_cast<uint32_t>(m_images.size());
		std::atomic_bool textureIssue = false;
		std::atomic_int imageIndex = 0;
		std::for_each(
			std::execution::par,
			m_images.begin(),
			m_images.end(),
			[&imageIndex, &textureIssue, &totalImageCount, asyncData, compress, imageSubTicks](std::shared_ptr<Image>& image)
			{
				int32_t idx = imageIndex++;

				if (textureIssue || image.get() == nullptr)
				{
					return;
				}

				if (asyncData != nullptr && asyncData->State == AsyncState::Cancelled)
				{
					return;
				}

				if (!image->Optimise(compress, true, asyncData))
				{
					textureIssue = true;
					return;
				}

				asyncData->AddSubProgress(imageSubTicks);
			});

		if (textureIssue)
		{
			if (asyncData == nullptr || asyncData->State != AsyncState::Cancelled)
				Logger::Error("Issue occurred during texture generation.");
			return false;
		}

		for (size_t i = 0; i < m_images.size(); ++i)
		{
			std::shared_ptr<Image>& image = m_images[i];
			if (image.get() == nullptr)
			{
				continue;
			}

			uint32_t componentCount = image->GetComponentCount();
			Format format;
			if (image->IsNormalMap() || image->IsMetallicRoughnessMap())
			{
				format = image->IsCompressed() ? Format::Bc5UnormBlock : Format::R8G8B8A8Unorm;
			}
			else if (componentCount == 4)
			{
				bool srgb = image->IsSRGB();
				if (image->IsCompressed())
				{
					format = srgb ? Format::Bc7SrgbBlock : Format::Bc7UnormBlock;
				}
				else
				{
					format = srgb ? Format::R8G8B8A8Srgb : Format::R8G8B8A8Unorm;
				}
			}
			else
			{
				Logger::Error("Images without exactly 4 channels are currently not supported.");
				return false;
			}

			const std::vector<std::vector<uint8_t>>& pixels = image->GetPixels();
			const glm::uvec2& size = image->GetSize();
			glm::uvec3 dimensions(size.x, size.y, 1);

			std::unique_ptr<IRenderImage>& renderImage = m_imageArray.emplace_back(std::move(resourceFactory.CreateRenderImage()));
			bool imageInitialised = renderImage->Initialise("SceneImage", device, ImageType::e2D, format, dimensions,
				static_cast<uint32_t>(pixels.size()), 1, ImageTiling::Optimal,
				ImageUsageFlags::TransferSrc | ImageUsageFlags::TransferDst | ImageUsageFlags::Sampled, ImageAspectFlags::Color,
				MemoryUsage::AutoPreferDevice, AllocationCreateFlags::None, SharingMode::Exclusive);

			if (!imageInitialised)
			{
				return false;
			}

			renderImage->TransitionImageLayout(device, commandBuffer, ImageLayout::TransferDst);

			for (size_t i = 0; i < pixels.size(); ++i)
			{
				if (!CreateImageStagingBuffer(device, resourceFactory, commandBuffer, renderImage.get(), static_cast<uint32_t>(i),
					pixels[i].data(), pixels[i].size(), temporaryBuffers))
					return false;
			}

			if (chunkData)
			{
				ImageHeader header;
				header.Width = static_cast<uint32_t>(size.x);
				header.Height = static_cast<uint32_t>(size.y);
				header.Format = static_cast<uint32_t>(format);
				chunkData->AddImageData(header, pixels);
			}

			renderImage->TransitionImageLayout(device, commandBuffer, ImageLayout::ShaderReadOnly);

			++imageCount;

			image.reset();
		}

		commandBuffer.MemoryBarrier(MaterialStageFlags::Transfer, MaterialAccessFlags::MemoryWrite,
			MaterialStageFlags::FragmentShader, MaterialAccessFlags::ShaderRead);

		return true;
	}

	bool GeometryBatch::UploadMeshInfoBuffer(const IDevice& device, const ICommandBuffer& commandBuffer, const IResourceFactory& resourceFactory,
		std::vector<std::unique_ptr<IBuffer>>& temporaryBuffers, IBuffer* buffer, const void* data, size_t dataSize)
	{
		bool initialised = buffer->Initialise("meshInfoBuffer", device, dataSize,
			BufferUsageFlags::TransferDst | BufferUsageFlags::StorageBuffer,
			MemoryUsage::AutoPreferDevice,
			AllocationCreateFlags::None,
			SharingMode::Exclusive);

		if (!initialised)
		{
			return false;
		}

		if (!CreateStagingBuffer(device, resourceFactory, commandBuffer, buffer, data,
			dataSize, temporaryBuffers))
			return false;

		commandBuffer.MemoryBarrier(MaterialStageFlags::Transfer, MaterialAccessFlags::MemoryWrite,
			MaterialStageFlags::VertexShader, MaterialAccessFlags::ShaderRead);

		return true;
	}

	bool GeometryBatch::SetupMeshInfoBuffer(const IDevice& device, const ICommandBuffer& commandBuffer,
		ChunkData* chunkData, std::vector<std::unique_ptr<IBuffer>>& temporaryBuffers, const IResourceFactory& resourceFactory)
	{
		if (chunkData != nullptr && chunkData->LoadedFromDisk())
		{
			ChunkMemoryEntry entry;
			if (!chunkData->GetGenericData(static_cast<uint32_t>(CachedDataType::MeshInfo), entry))
			{
				return false;
			}

			std::vector<uint8_t> decompressBuffer;
			chunkData->Decompress(entry, decompressBuffer);

			m_meshInfoBuffer = std::move(resourceFactory.CreateBuffer());

			return UploadMeshInfoBuffer(device, commandBuffer, resourceFactory, temporaryBuffers,
				m_meshInfoBuffer.get(), decompressBuffer.data(), decompressBuffer.size());
		}

		uint64_t totalSize = 0;
		for (uint32_t i = 0; i < m_meshCapacity; ++i)
		{
			if (!m_active[i])
				continue;

			totalSize += sizeof(RenderMeshInfo);
		}

		std::vector<uint8_t> uniformBufferData;
		uniformBufferData.resize(totalSize);
		totalSize = 0;

		for (uint32_t i = 0; i < m_meshCapacity; ++i)
		{
			if (!m_active[i])
				continue;

			const MeshInfo& meshInfo = m_meshInfos[i];
			RenderMeshInfo data = {};
			data.transform = meshInfo.transform;
			data.normalMatrix = glm::mat4(glm::transpose(glm::inverse(glm::mat3(meshInfo.transform))));
			data.colour = meshInfo.colour.GetVec4();
			data.diffuseImageIndex = static_cast<uint32_t>(meshInfo.diffuseImageIndex);
			data.normalImageIndex = static_cast<uint32_t>(meshInfo.normalImageIndex);
			data.metallicRoughnessImageIndex = static_cast<uint32_t>(meshInfo.metallicRoughnessImageIndex);
			memcpy(uniformBufferData.data() + totalSize, &data, sizeof(RenderMeshInfo));
			totalSize += sizeof(RenderMeshInfo);
		}

		if (chunkData)
			chunkData->SetGenericData(static_cast<uint32_t>(CachedDataType::MeshInfo), uniformBufferData);

		m_meshInfoBuffer = std::move(resourceFactory.CreateBuffer());

		return UploadMeshInfoBuffer(device, commandBuffer, resourceFactory, temporaryBuffers,
			m_meshInfoBuffer.get(), uniformBufferData.data(), uniformBufferData.size());
	}

	bool GeometryBatch::Build(ChunkData* chunkData, AsyncData& asyncData)
	{
		// TODO: Handle resizing
		if (m_indexBuffer.get() != nullptr)
		{
			Logger::Error("Rebuilding existing scene render data currently not supported.");
			asyncData.State = AsyncState::Failed;
			return false;
		}

		auto startTime = std::chrono::high_resolution_clock::now();

		return m_renderer.SubmitResourceCommand([this, chunkData, &asyncData](const IDevice& device, const IPhysicalDevice& physicalDevice,
			const ICommandBuffer& commandBuffer, std::vector<std::unique_ptr<IBuffer>>& temporaryBuffers)
			{
				const IResourceFactory& resourceFactory = m_renderer.GetResourceFactory();

				uint32_t imageCount;
				if (!SetupVertexBuffers(device, commandBuffer, chunkData, temporaryBuffers, resourceFactory))
				{
					return false;
				}

				asyncData.AddSubProgress(50.0f);

				if (!SetupIndexBuffer(device, commandBuffer, chunkData, temporaryBuffers, resourceFactory))
				{
					return false;
				}

				asyncData.AddSubProgress(50.0f);

				if (!SetupRenderImage(&asyncData, device, physicalDevice, commandBuffer, chunkData, temporaryBuffers, resourceFactory, physicalDevice.GetMaxAnisotropy(), imageCount)
					|| !SetupMeshInfoBuffer(device, commandBuffer, chunkData, temporaryBuffers, resourceFactory)
					|| !SetupIndirectDrawBuffer(device, commandBuffer, chunkData, temporaryBuffers, resourceFactory)
					|| !SetupBoundsBuffer(device, commandBuffer, chunkData, temporaryBuffers, resourceFactory))
				{
					if (asyncData.State != AsyncState::Cancelled)
						asyncData.State = AsyncState::Failed;
					return false;
				}

				return true;
			}, [this, startTime]()
				{
					m_creating = false;
					auto endTime = std::chrono::high_resolution_clock::now();
					float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(endTime - startTime).count();
					Logger::Verbose("Scene manager build finished in {} seconds.", deltaTime);

					// Rebuild render graph when batch has loaded.
					m_renderer.GetRenderGraph().MarkDirty();
				});
	}
}