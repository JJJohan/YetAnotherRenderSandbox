#include "GeometryBatch.hpp"
#include "../Resources/IPhysicalDevice.hpp"
#include "../Resources/IDevice.hpp"
#include "../Resources/IBuffer.hpp"
#include "../Resources/IRenderImage.hpp"
#include "../Resources/IImageSampler.hpp"
#include "../Resources/IResourceFactory.hpp"
#include "../Resources/IMaterialManager.hpp"
#include "../Resources/ICommandBuffer.hpp"
#include "../Resources/Material.hpp"
#include "../MeshInfo.hpp"
#include "Core/MeshOptimiser.hpp"
#include "Core/Image.hpp"
#include "VulkanRenderer.hpp"
#include "../RenderMeshInfo.hpp"
#include "../GBuffer.hpp"
#include "Core/Logging/Logger.hpp"
#include "Core/ChunkData.hpp"
#include "Core/AsyncData.hpp"
#include "Core/VertexData.hpp"
#include "Core/Colour.hpp"
#include <execution>

using namespace Engine::Logging;
using namespace Engine::OS;

namespace Engine::Rendering::Vulkan
{
	GeometryBatch::GeometryBatch(VulkanRenderer& renderer)
		: m_renderer(renderer)
		, m_sampler()
		, m_indirectDrawBuffer()
		, m_vertexBuffers()
		, m_indexBuffer()
		, m_meshInfoBuffer()
		, m_imageArray()
		, m_indirectDrawCommands()
		, m_vertexOffsets()
		, m_indexOffsets()
		, m_indexCounts()
		, m_shadowMaterial(nullptr)
		, m_pbrMaterial(nullptr)
		, m_recycledIds()
		, m_active()
		, m_meshCapacity()
		, m_vertexDataArrays()
		, m_indexArrays()
		, m_meshInfos()
		, m_images()
		, m_vertexDataHashTable()
		, m_imageHashTable()
		, m_creating(true)
	{
	}

	bool GeometryBatch::CreateMesh(const std::vector<VertexData>& vertexData,
		const std::vector<uint32_t>& indices,
		const glm::mat4& transform,
		const Colour& colour,
		std::shared_ptr<Image> diffuseImage,
		std::shared_ptr<Image> normalImage,
		std::shared_ptr<Image> metallicRoughnessImage)
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

	bool GeometryBatch::Initialise(const IPhysicalDevice& physicalDevice, const IDevice& device,
		const IResourceFactory& resourceFactory, const IMaterialManager& materialManager)
	{
		m_sampler = std::move(resourceFactory.CreateImageSampler());
		bool samplerInitialised = m_sampler->Initialise(device, Filter::Linear, Filter::Linear,
			SamplerMipmapMode::Linear, SamplerAddressMode::Repeat, physicalDevice.GetMaxAnisotropy());
		if (!samplerInitialised)
		{
			return false;
		}

		if (!materialManager.TryGetMaterial("PBR", &m_pbrMaterial) ||
			!materialManager.TryGetMaterial("Shadow", &m_shadowMaterial))
		{
			return false;
		}

		return true;
	}

	bool GeometryBatch::SetupIndirectDrawBuffer(const ICommandBuffer& commandBuffer,
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
			IBuffer* buffer = m_indirectDrawBuffer.get();

			size_t totalSize = decompressBuffer.size();
			bool initialised = buffer->Initialise(totalSize,
				BufferUsageFlags::TransferDst | BufferUsageFlags::IndirectBuffer,
				MemoryUsage::AutoPreferDevice,
				AllocationCreateFlags::None,
				SharingMode::Exclusive);

			if (!initialised)
			{
				return false;
			}

			if (!CreateStagingBuffer(resourceFactory, commandBuffer, buffer, decompressBuffer.data(),
				totalSize, temporaryBuffers))
				return false;

			m_meshInfos.resize(totalSize / sizeof(IndexedIndirectCommand)); // TODO: Cleanup
			m_meshCapacity = static_cast<uint32_t>(m_meshInfos.size());
			return true;
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
		IBuffer* buffer = m_indirectDrawBuffer.get();

		size_t totalSize = indirectBufferData.size() * sizeof(IndexedIndirectCommand);
		bool initialised = buffer->Initialise(totalSize,
			BufferUsageFlags::TransferDst | BufferUsageFlags::IndirectBuffer,
			MemoryUsage::AutoPreferDevice,
			AllocationCreateFlags::None,
			SharingMode::Exclusive);

		if (!initialised)
		{
			return false;
		}

		if (!CreateStagingBuffer(resourceFactory, commandBuffer, buffer, indirectBufferData.data(),
			totalSize, temporaryBuffers))
			return false;

		if (chunkData)
		{
			std::span<uint8_t> cacheData(reinterpret_cast<uint8_t*>(indirectBufferData.data()), totalSize);
			chunkData->SetGenericData(static_cast<uint32_t>(CachedDataType::IndirectDrawBuffer), cacheData);
		}

		return true;
	}

	bool GeometryBatch::SetupVertexBuffers(const ICommandBuffer& commandBuffer,
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

				bool initialised = buffer->Initialise(span.size(),
					BufferUsageFlags::TransferDst | BufferUsageFlags::VertexBuffer,
					MemoryUsage::AutoPreferDevice,
					AllocationCreateFlags::None,
					SharingMode::Exclusive);

				if (!initialised)
				{
					return false;
				}

				if (!CreateStagingBuffer(resourceFactory, commandBuffer, buffer, span.data(),
					span.size(), temporaryBuffers))
					return false;
			}

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
				memcpy(vertexBufferData.data() + totalSize, data->GetData(), size);

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

			bool initialised = buffer->Initialise(totalSize,
				BufferUsageFlags::TransferDst | BufferUsageFlags::VertexBuffer,
				MemoryUsage::AutoPreferDevice,
				AllocationCreateFlags::None,
				SharingMode::Exclusive);

			if (!initialised)
			{
				return false;
			}

			if (!CreateStagingBuffer(resourceFactory, commandBuffer, buffer, vertexBufferData.data(),
				totalSize, temporaryBuffers))
				return false;
		}

		return true;
	}

	bool GeometryBatch::SetupIndexBuffer(const ICommandBuffer& commandBuffer,
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
			IBuffer* buffer = m_indexBuffer.get();

			bool initialised = buffer->Initialise(decompressBuffer.size(),
				BufferUsageFlags::TransferDst | BufferUsageFlags::IndexBuffer,
				MemoryUsage::AutoPreferDevice,
				AllocationCreateFlags::None,
				SharingMode::Exclusive);

			if (!initialised)
			{
				return false;
			}

			if (!CreateStagingBuffer(resourceFactory, commandBuffer, buffer, decompressBuffer.data(),
				decompressBuffer.size(), temporaryBuffers))
				return false;

			return true;
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

		if (chunkData)
			chunkData->SetGenericData(static_cast<uint32_t>(CachedDataType::IndexBuffer), indexBufferData);

		m_indexBuffer = std::move(resourceFactory.CreateBuffer());
		IBuffer* buffer = m_indexBuffer.get();

		bool initialised = buffer->Initialise(totalSize,
			BufferUsageFlags::TransferDst | BufferUsageFlags::IndexBuffer,
			MemoryUsage::AutoPreferDevice,
			AllocationCreateFlags::None,
			SharingMode::Exclusive);

		if (!initialised)
		{
			return false;
		}

		if (!CreateStagingBuffer(resourceFactory, commandBuffer, buffer, indexBufferData.data(),
			totalSize, temporaryBuffers))
			return false;

		return true;
	}

	bool GeometryBatch::CreateStagingBuffer(const IResourceFactory& resourceFactory,
		const ICommandBuffer& commandBuffer, const IBuffer* destinationBuffer, const void* data, uint64_t size,
		std::vector<std::unique_ptr<IBuffer>>& copyBufferCollection)
	{
		IBuffer* stagingBuffer = copyBufferCollection.emplace_back(std::move(resourceFactory.CreateBuffer())).get();
		if (!stagingBuffer->Initialise(size,
			BufferUsageFlags::TransferSrc, MemoryUsage::Auto,
			AllocationCreateFlags::HostAccessSequentialWrite | AllocationCreateFlags::Mapped,
			SharingMode::Exclusive))
		{
			return false;
		}

		if (!stagingBuffer->UpdateContents(data, size))
			return false;

		stagingBuffer->Copy(commandBuffer, *destinationBuffer, size);

		return true;
	}

	bool GeometryBatch::CreateImageStagingBuffer(const IResourceFactory& resourceFactory,
		const ICommandBuffer& commandBuffer, const IRenderImage* destinationImage, uint32_t mipLevel, const void* data, uint64_t size,
		std::vector<std::unique_ptr<IBuffer>>& copyBufferCollection)
	{
		IBuffer* stagingBuffer = copyBufferCollection.emplace_back(std::move(resourceFactory.CreateBuffer())).get();
		if (!stagingBuffer->Initialise(size,
			BufferUsageFlags::TransferSrc, MemoryUsage::Auto,
			AllocationCreateFlags::HostAccessSequentialWrite | AllocationCreateFlags::Mapped,
			SharingMode::Exclusive))
		{
			return false;
		}

		if (!stagingBuffer->UpdateContents(data, size))
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
				bool imageInitialised = renderImage->Initialise(device, ImageType::e2D, format, dimensions, imageData.Header.MipLevels, ImageTiling::Optimal,
					ImageUsageFlags::TransferSrc | ImageUsageFlags::TransferDst | ImageUsageFlags::Sampled, ImageAspectFlags::Color,
					MemoryUsage::AutoPreferDevice,
					AllocationCreateFlags::None,
					SharingMode::Exclusive);

				if (!imageInitialised)
				{
					return false;
				}

				renderImage->TransitionImageLayout(device, commandBuffer, ImageLayout::TransferDst);

				for (uint32_t i = 0; i < imageData.Header.MipLevels; ++i)
				{
					if (!CreateImageStagingBuffer(resourceFactory, commandBuffer, renderImage.get(), i, spans[i].data(),
						spans[i].size(), temporaryBuffers))
						return false;
				}

				renderImage->TransitionImageLayout(device, commandBuffer, ImageLayout::ShaderReadOnly);

				if (asyncData != nullptr)
					asyncData->AddSubProgress(subTicks);
			}

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
			bool imageInitialised = renderImage->Initialise(device, ImageType::e2D, format, dimensions, static_cast<uint32_t>(pixels.size()), ImageTiling::Optimal,
				ImageUsageFlags::TransferSrc | ImageUsageFlags::TransferDst | ImageUsageFlags::Sampled, ImageAspectFlags::Color,
				MemoryUsage::AutoPreferDevice,
				AllocationCreateFlags::None,
				SharingMode::Exclusive);

			if (!imageInitialised)
			{
				return false;
			}

			renderImage->TransitionImageLayout(device, commandBuffer, ImageLayout::TransferDst);

			for (size_t i = 0; i < pixels.size(); ++i)
			{
				if (!CreateImageStagingBuffer(resourceFactory, commandBuffer, renderImage.get(), static_cast<uint32_t>(i),
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

		return true;
	}

	bool GeometryBatch::SetupMeshInfoBuffer(const ICommandBuffer& commandBuffer,
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
			IBuffer* buffer = m_meshInfoBuffer.get();

			bool initialised = buffer->Initialise(decompressBuffer.size(),
				BufferUsageFlags::TransferDst | BufferUsageFlags::StorageBuffer,
				MemoryUsage::AutoPreferDevice,
				AllocationCreateFlags::None,
				SharingMode::Exclusive);

			if (!initialised)
			{
				return false;
			}

			if (!CreateStagingBuffer(resourceFactory, commandBuffer, buffer, decompressBuffer.data(),
				decompressBuffer.size(), temporaryBuffers))
				return false;

			return true;
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
		IBuffer* buffer = m_meshInfoBuffer.get();

		bool initialised = buffer->Initialise(totalSize,
			BufferUsageFlags::TransferDst | BufferUsageFlags::StorageBuffer,
			MemoryUsage::AutoPreferDevice,
			AllocationCreateFlags::None,
			SharingMode::Exclusive);

		if (!initialised)
		{
			return false;
		}

		if (!CreateStagingBuffer(resourceFactory, commandBuffer, buffer, uniformBufferData.data(),
			totalSize, temporaryBuffers))
			return false;

		return true;
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
				const GBuffer& gBuffer = m_renderer.GetGBuffer();
				uint32_t concurrentFrames = m_renderer.GetConcurrentFrameCount();
				const IResourceFactory& resourceFactory = m_renderer.GetResourceFactory();
				const std::vector<std::unique_ptr<IBuffer>>& frameInfoBuffers = m_renderer.GetFrameInfoBuffers();
				const std::vector<std::unique_ptr<IBuffer>>& lightBuffers = m_renderer.GetLightBuffers();

				uint32_t imageCount;
				if (!SetupVertexBuffers(commandBuffer, chunkData, temporaryBuffers, resourceFactory))
				{
					return false;
				}

				asyncData.AddSubProgress(50.0f);

				if (!SetupIndexBuffer(commandBuffer, chunkData, temporaryBuffers, resourceFactory))
				{
					return false;
				}

				asyncData.AddSubProgress(50.0f);

				if (!SetupRenderImage(&asyncData, device, physicalDevice, commandBuffer, chunkData, temporaryBuffers, resourceFactory, physicalDevice.GetMaxAnisotropy(), imageCount)
					|| !SetupMeshInfoBuffer(commandBuffer, chunkData, temporaryBuffers, resourceFactory)
					|| !SetupIndirectDrawBuffer(commandBuffer, chunkData, temporaryBuffers, resourceFactory))
				{
					if (asyncData.State != AsyncState::Cancelled)
						asyncData.State = AsyncState::Failed;
					return false;
				}

				if (asyncData.State == AsyncState::Cancelled)
				{
					return false;
				}

				std::vector<const IImageView*> imageViews(m_imageArray.size());
				for (size_t i = 0; i < m_imageArray.size(); ++i)
					imageViews[i] = &m_imageArray[i]->GetView();

				if (!m_pbrMaterial->BindUniformBuffers(0, frameInfoBuffers) ||
					!m_pbrMaterial->BindStorageBuffer(1, m_meshInfoBuffer) ||
					!m_pbrMaterial->BindSampler(2, m_sampler) ||
					!m_pbrMaterial->BindImageViews(3, imageViews))
					return false;

				if (!m_shadowMaterial->BindUniformBuffers(0, frameInfoBuffers) ||
					!m_shadowMaterial->BindUniformBuffers(1, lightBuffers) ||
					!m_shadowMaterial->BindStorageBuffer(2, m_meshInfoBuffer) ||
					!m_shadowMaterial->BindSampler(3, m_sampler) ||
					!m_shadowMaterial->BindImageViews(4, imageViews))
					return false;

				return true;
			}, [this, startTime]()
				{
					m_creating = false;
					auto endTime = std::chrono::high_resolution_clock::now();
					float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(endTime - startTime).count();
					Logger::Verbose("Scene manager build finished in {} seconds.", deltaTime);
				});
	}

	void GeometryBatch::Draw(const ICommandBuffer& commandBuffer, uint32_t currentFrameIndex)
	{
		if (m_vertexBuffers.empty() || m_creating)
			return;

		m_pbrMaterial->BindMaterial(commandBuffer, currentFrameIndex);
		uint32_t vertexBufferCount = static_cast<uint32_t>(m_vertexBuffers.size());

		std::vector<size_t> vertexBufferOffsets;
		vertexBufferOffsets.resize(vertexBufferCount);
		std::vector<IBuffer*> vertexBufferViews;
		vertexBufferViews.reserve(vertexBufferCount);
		for (const auto& buffer : m_vertexBuffers)
		{
			vertexBufferViews.emplace_back(buffer.get());
		}

		commandBuffer.BindVertexBuffers(0, vertexBufferViews, vertexBufferOffsets);
		commandBuffer.BindIndexBuffer(*m_indexBuffer, 0, IndexType::Uint32);

		uint32_t drawCount = m_meshCapacity; // TODO: Compute counted, after culling, etc.
		commandBuffer.DrawIndexedIndirect(*m_indirectDrawBuffer, 0, drawCount, sizeof(IndexedIndirectCommand));
	}

	void GeometryBatch::DrawShadows(const ICommandBuffer& commandBuffer, uint32_t currentFrameIndex, uint32_t cascadeIndex)
	{
		if (m_vertexBuffers.empty() || m_creating)
			return;

		commandBuffer.PushConstants(m_shadowMaterial, ShaderStageFlags::Vertex, 0, sizeof(uint32_t), &cascadeIndex);

		if (cascadeIndex == 0)
		{
			std::vector<size_t> vertexBufferOffsets;
			vertexBufferOffsets.resize(2);
			std::vector<IBuffer*> vertexBufferViews = { m_vertexBuffers[0].get(), m_vertexBuffers[1].get()};

			m_shadowMaterial->BindMaterial(commandBuffer, currentFrameIndex);
			commandBuffer.BindVertexBuffers(0, vertexBufferViews, vertexBufferOffsets);
			commandBuffer.BindIndexBuffer(*m_indexBuffer, 0, IndexType::Uint32);
		}

		uint32_t drawCount = m_meshCapacity; // TODO: Compute counted, after culling, etc.
		commandBuffer.DrawIndexedIndirect(*m_indirectDrawBuffer, 0, drawCount, sizeof(IndexedIndirectCommand));
	}
}