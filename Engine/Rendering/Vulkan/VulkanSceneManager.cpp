#include "VulkanSceneManager.hpp"
#include "PhysicalDevice.hpp"
#include "Device.hpp"
#include "CommandBuffer.hpp"
#include "DescriptorPool.hpp"
#include "Buffer.hpp"
#include "RenderImage.hpp"
#include "ImageView.hpp"
#include "ImageSampler.hpp"
#include "RenderPass.hpp"
#include "VulkanRenderer.hpp"
#include "CommandPool.hpp"
#include "FrameInfoUniformBuffer.hpp"
#include "RenderMeshInfo.hpp"
#include "PipelineLayout.hpp"
#include "Core/Logging/Logger.hpp"
#include "Core/ChunkData.hpp"
#include "Core/AsyncData.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vma/vk_mem_alloc.h>
#include <algorithm>
#include <execution>

using namespace Engine::Logging;

namespace Engine::Rendering::Vulkan
{
	VulkanSceneManager::VulkanSceneManager(VulkanRenderer& renderer)
		: m_renderer(renderer)
		, m_descriptorPool()
		, m_descriptorSets()
		, m_blankImage()
		, m_blankImageView()
		, m_sampler()
		, m_indirectDrawBuffer()
		, m_vertexBuffers()
		, m_indexBuffer()
		, m_meshInfoBuffer()
		, m_imageArray()
		, m_imageArrayView()
		, m_indirectDrawCommands()
		, m_vertexOffsets()
		, m_indexOffsets()
		, m_indexCounts()
	{
	}

	bool VulkanSceneManager::Initialise(Shader* shader)
	{
		m_shader = shader;

		const Device& device = m_renderer.GetDevice();
		const PhysicalDevice& physicalDevice = m_renderer.GetPhysicalDevice();
		VmaAllocator allocator = m_renderer.GetAllocator();
		uint32_t concurrentFrames = m_renderer.GetConcurrentFrameCount();

		m_sampler = std::make_unique<ImageSampler>();
		bool samplerInitialised = m_sampler->Initialise(device, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear, vk::SamplerAddressMode::eRepeat, physicalDevice.GetLimits().maxSamplerAnisotropy);
		if (!samplerInitialised)
		{
			return false;
		}

		m_blankImage = std::make_shared<RenderImage>(allocator);
		bool imageInitialised = m_blankImage->Initialise(vk::ImageType::e2D, vk::Format::eR8G8B8A8Srgb, vk::Extent3D(1, 1, 1), vk::SampleCountFlagBits::e1, 1, vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
			VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
			0,
			vk::SharingMode::eExclusive);
		if (!imageInitialised)
		{
			return false;
		}

		m_blankImageView = std::make_shared<ImageView>();
		bool imageViewInitialised = m_blankImageView->Initialise(device, m_blankImage->Get(), 1, m_blankImage->GetFormat(), vk::ImageAspectFlagBits::eColor);
		if (!imageViewInitialised)
		{
			return false;
		}

		std::vector<vk::DescriptorPoolSize> poolSizes =
		{
			vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, concurrentFrames),
			vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, concurrentFrames),
			vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, concurrentFrames),
		};

		m_descriptorPool = std::make_unique<DescriptorPool>();
		if (!m_descriptorPool->Initialise(device, concurrentFrames, poolSizes))
		{
			return false;
		}

		return m_renderer.SubmitResourceCommand([this, &device, &allocator](const vk::CommandBuffer& commandBuffer, std::vector<std::unique_ptr<Buffer>>& temporaryBuffers)
			{
				m_blankImage->TransitionImageLayout(device, commandBuffer, vk::ImageLayout::eTransferDstOptimal);

				Colour blankPixel = Colour();
				if (!CreateImageStagingBuffer(allocator, device, commandBuffer, m_blankImage.get(), 0, &blankPixel,
					sizeof(uint32_t), temporaryBuffers))
					return false;

				m_blankImage->TransitionImageLayout(device, commandBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
				return true;
			});
	}

	bool VulkanSceneManager::SetupIndirectDrawBuffer(const Device& device, const vk::CommandBuffer& commandBuffer,
		ChunkData* chunkData, std::vector<std::unique_ptr<Buffer>>& temporaryBuffers, VmaAllocator allocator)
	{
		if (chunkData != nullptr && chunkData->LoadedFromDisk())
		{
			ChunkMemoryEntry entry;
			if (!chunkData->GetGenericData(2, entry))
			{
				return false;
			}

			std::vector<uint8_t> decompressBuffer;
			chunkData->Decompress(entry, decompressBuffer);

			m_indirectDrawBuffer = std::make_unique<Buffer>(allocator);
			Buffer* buffer = m_indirectDrawBuffer.get();

			size_t totalSize = decompressBuffer.size();
			bool initialised = buffer->Initialise(totalSize,
				vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndirectBuffer,
				VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
				0,
				vk::SharingMode::eExclusive);

			if (!initialised)
			{
				return false;
			}

			if (!CreateStagingBuffer(allocator, device, commandBuffer, buffer, decompressBuffer.data(),
				totalSize, temporaryBuffers))
				return false;

			m_meshInfos.resize(totalSize / sizeof(vk::DrawIndexedIndirectCommand)); // TODO: Cleanup
			m_meshCapacity = static_cast<uint32_t>(m_meshInfos.size());
			return true;
		}

		std::vector<vk::DrawIndexedIndirectCommand> indirectBufferData;
		indirectBufferData.reserve(m_meshCapacity);
		for (size_t i = 0; i < m_meshCapacity; ++i)
		{
			if (!m_active[i])
				continue;

			const MeshInfo& meshInfo = m_meshInfos[i];
			vk::DrawIndexedIndirectCommand indirectCommand{};
			indirectCommand.vertexOffset = m_vertexOffsets[meshInfo.vertexBufferIndex];
			indirectCommand.firstIndex = m_indexOffsets[meshInfo.indexBufferIndex];
			indirectCommand.indexCount = m_indexCounts[meshInfo.indexBufferIndex];
			indirectCommand.instanceCount = 1;
			indirectBufferData.emplace_back(indirectCommand);
		}

		m_indirectDrawBuffer = std::make_unique<Buffer>(allocator);
		Buffer* buffer = m_indirectDrawBuffer.get();

		size_t totalSize = indirectBufferData.size() * sizeof(vk::DrawIndexedIndirectCommand);
		bool initialised = buffer->Initialise(totalSize,
			vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndirectBuffer,
			VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
			0,
			vk::SharingMode::eExclusive);

		if (!initialised)
		{
			return false;
		}

		if (!CreateStagingBuffer(allocator, device, commandBuffer, buffer, indirectBufferData.data(),
			totalSize, temporaryBuffers))
			return false;

		if (chunkData)
		{
			// TODO: Cast?
			std::vector<uint8_t> cacheData(totalSize);
			memcpy(cacheData.data(), indirectBufferData.data(), totalSize);
			chunkData->SetGenericData(2, cacheData);
		}

		return true;
	}

	bool VulkanSceneManager::SetupVertexBuffers(const Device& device, const vk::CommandBuffer& commandBuffer,
		ChunkData* chunkData, std::vector<std::unique_ptr<Buffer>>& temporaryBuffers, VmaAllocator allocator)
	{
		if (chunkData != nullptr && chunkData->LoadedFromDisk())
		{
			std::array<ChunkMemoryEntry, 5> cacheEntries;
			if (!chunkData->GetVertexData(VertexBufferType::Positions, cacheEntries[0]))
				return false;
			if (!chunkData->GetVertexData(VertexBufferType::TextureCoordinates, cacheEntries[1]))
				return false;
			if (!chunkData->GetVertexData(VertexBufferType::Normals, cacheEntries[2]))
				return false;
			if (!chunkData->GetVertexData(VertexBufferType::Tangents, cacheEntries[3]))
				return false;
			if (!chunkData->GetVertexData(VertexBufferType::Bitangents, cacheEntries[4]))
				return false;

			std::vector<uint8_t> decompressBuffer;
			m_vertexBuffers.resize(cacheEntries.size());
			for (size_t i = 0; i < cacheEntries.size(); ++i)
			{
				m_vertexBuffers[i] = std::make_unique<Buffer>(allocator);
				Buffer* buffer = m_vertexBuffers[i].get();

				chunkData->Decompress(cacheEntries[i], decompressBuffer);
				const std::span<const uint8_t> span(decompressBuffer.begin(), cacheEntries[i].UncompressedSize);

				bool initialised = buffer->Initialise(span.size(),
					vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
					VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
					0,
					vk::SharingMode::eExclusive);

				if (!initialised)
				{
					return false;
				}

				if (!CreateStagingBuffer(allocator, device, commandBuffer, buffer, span.data(),
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
				else if (vertexBit == 3)
					chunkData->SetVertexData(VertexBufferType::Tangents, vertexBufferData);
				else if (vertexBit == 4)
					chunkData->SetVertexData(VertexBufferType::Bitangents, vertexBufferData);
				else
					throw;
			}

			m_vertexBuffers[vertexBit] = std::make_unique<Buffer>(allocator);
			Buffer* buffer = m_vertexBuffers[vertexBit].get();

			bool initialised = buffer->Initialise(totalSize,
				vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
				VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
				0,
				vk::SharingMode::eExclusive);

			if (!initialised)
			{
				return false;
			}

			if (!CreateStagingBuffer(allocator, device, commandBuffer, buffer, vertexBufferData.data(),
				totalSize, temporaryBuffers))
				return false;
		}

		return true;
	}

	bool VulkanSceneManager::SetupIndexBuffer(const Device& device, const vk::CommandBuffer& commandBuffer,
		ChunkData* chunkData, std::vector<std::unique_ptr<Buffer>>& temporaryBuffers, VmaAllocator allocator)
	{
		if (chunkData != nullptr && chunkData->LoadedFromDisk())
		{
			ChunkMemoryEntry entry;
			if (!chunkData->GetGenericData(0, entry))
			{
				return false;
			}

			std::vector<uint8_t> decompressBuffer;
			chunkData->Decompress(entry, decompressBuffer);

			m_indexBuffer = std::make_unique<Buffer>(allocator);
			Buffer* buffer = m_indexBuffer.get();

			bool initialised = buffer->Initialise(decompressBuffer.size(),
				vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
				VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
				0,
				vk::SharingMode::eExclusive);

			if (!initialised)
			{
				return false;
			}

			if (!CreateStagingBuffer(allocator, device, commandBuffer, buffer, decompressBuffer.data(),
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
			chunkData->SetGenericData(0, indexBufferData);

		m_indexBuffer = std::make_unique<Buffer>(allocator);
		Buffer* buffer = m_indexBuffer.get();

		bool initialised = buffer->Initialise(totalSize,
			vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
			VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
			0,
			vk::SharingMode::eExclusive);

		if (!initialised)
		{
			return false;
		}

		if (!CreateStagingBuffer(allocator, device, commandBuffer, buffer, indexBufferData.data(),
			totalSize, temporaryBuffers))
			return false;

		return true;
	}

	bool VulkanSceneManager::CreateStagingBuffer(VmaAllocator allocator, const Device& device,
		const vk::CommandBuffer& commandBuffer, const Buffer* destinationBuffer, const void* data, uint64_t size,
		std::vector<std::unique_ptr<Buffer>>& copyBufferCollection)
	{
		Buffer* stagingBuffer = copyBufferCollection.emplace_back(std::make_unique<Buffer>(allocator)).get();
		if (!stagingBuffer->Initialise(size,
			vk::BufferUsageFlagBits::eTransferSrc,
			VMA_MEMORY_USAGE_AUTO,
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
			vk::SharingMode::eExclusive))
		{
			return false;
		}

		if (!stagingBuffer->UpdateContents(data, size))
			return false;

		stagingBuffer->Copy(device, commandBuffer, *destinationBuffer, size);

		return true;
	}

	bool VulkanSceneManager::CreateImageStagingBuffer(VmaAllocator allocator, const Device& device,
		const vk::CommandBuffer& commandBuffer, const RenderImage* destinationImage, uint32_t mipLevel, const void* data, uint64_t size,
		std::vector<std::unique_ptr<Buffer>>& copyBufferCollection)
	{
		Buffer* stagingBuffer = copyBufferCollection.emplace_back(std::make_unique<Buffer>(allocator)).get();
		if (!stagingBuffer->Initialise(size,
			vk::BufferUsageFlagBits::eTransferSrc,
			VMA_MEMORY_USAGE_AUTO,
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
			vk::SharingMode::eExclusive))
		{
			return false;
		}

		if (!stagingBuffer->UpdateContents(data, size))
			return false;

		stagingBuffer->CopyToImage(device, mipLevel, commandBuffer, *destinationImage);


		return true;
	}

	bool VulkanSceneManager::SetupRenderImage(AsyncData* asyncData, const Device& device, const PhysicalDevice& physicalDevice,
		const vk::CommandBuffer& commandBuffer, ChunkData* chunkData, std::vector<std::unique_ptr<Buffer>>& temporaryBuffers,
		VmaAllocator allocator, float maxAnisotropy, uint32_t& imageCount)
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
			m_imageArrayView.reserve(cachedImageData->size());

			float subTicks = 400.0f / static_cast<float>(cachedImageData->size());

			std::vector<uint8_t> decompressBuffer;
			for (auto it = cachedImageData->begin(); it != cachedImageData->end(); ++it)
			{
				const ImageData& imageData = *it;
				vk::Format format = static_cast<vk::Format>(imageData.Header.Format);
				vk::Extent3D dimensions(imageData.Header.Width, imageData.Header.Height, 1);

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

				std::unique_ptr<RenderImage>& renderImage = m_imageArray.emplace_back(std::make_unique<RenderImage>(allocator));
				bool imageInitialised = renderImage->Initialise(vk::ImageType::e2D, format, dimensions, vk::SampleCountFlagBits::e1, imageData.Header.MipLevels, vk::ImageTiling::eOptimal,
					vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
					VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
					0,
					vk::SharingMode::eExclusive);

				if (!imageInitialised)
				{
					return false;
				}

				std::unique_ptr<ImageView>& imageView = m_imageArrayView.emplace_back(std::make_unique<ImageView>());
				bool imageViewInitialised = imageView->Initialise(device, renderImage->Get(), renderImage->GetMiplevels(), renderImage->GetFormat(), vk::ImageAspectFlagBits::eColor);
				if (!imageViewInitialised)
				{
					return false;
				}

				renderImage->TransitionImageLayout(device, commandBuffer, vk::ImageLayout::eTransferDstOptimal);

				for (uint32_t i = 0; i < imageData.Header.MipLevels; ++i)
				{
					if (!CreateImageStagingBuffer(allocator, device, commandBuffer, renderImage.get(), i, spans[i].data(),
						spans[i].size(), temporaryBuffers))
						return false;
				}

				renderImage->TransitionImageLayout(device, commandBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);

				if (asyncData != nullptr)
					asyncData->AddSubProgress(subTicks);
			}

			return true;
		}

		m_imageArray.reserve(m_images.size());
		m_imageArrayView.reserve(m_images.size());
		imageCount = 0;

		bool compress = false;
		const vk::PhysicalDeviceFeatures& features = physicalDevice.GetFeatures();
		if (features.textureCompressionBC && RenderImage::FormatSupported(physicalDevice, vk::Format::eBc7SrgbBlock))
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
			vk::Format format;
			if (image->IsNormalMap() || image->IsMetallicRoughnessMap())
			{
				format = image->IsCompressed() ? vk::Format::eBc5UnormBlock : vk::Format::eR8G8B8A8Unorm;
			}
			else if (componentCount == 4)
			{
				bool srgb = image->IsSRGB();
				if (image->IsCompressed())
				{
					format = srgb ? vk::Format::eBc7SrgbBlock : vk::Format::eBc7UnormBlock;
				}
				else
				{
					format = srgb ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm;
				}
			}
			else
			{
				Logger::Error("Images without exactly 4 channels are currently not supported.");
				return false;
			}

			const std::vector<std::vector<uint8_t>>& pixels = image->GetPixels();
			const glm::uvec2& size = image->GetSize();
			vk::Extent3D dimensions(size.x, size.y, 1);

			std::unique_ptr<RenderImage>& renderImage = m_imageArray.emplace_back(std::make_unique<RenderImage>(allocator));
			bool imageInitialised = renderImage->Initialise(vk::ImageType::e2D, format, dimensions, vk::SampleCountFlagBits::e1, static_cast<uint32_t>(pixels.size()), vk::ImageTiling::eOptimal,
				vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
				VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
				0,
				vk::SharingMode::eExclusive);

			if (!imageInitialised)
			{
				return false;
			}

			std::unique_ptr<ImageView>& imageView = m_imageArrayView.emplace_back(std::make_unique<ImageView>());
			bool imageViewInitialised = imageView->Initialise(device, renderImage->Get(), renderImage->GetMiplevels(), renderImage->GetFormat(), vk::ImageAspectFlagBits::eColor);
			if (!imageViewInitialised)
			{
				return false;
			}

			renderImage->TransitionImageLayout(device, commandBuffer, vk::ImageLayout::eTransferDstOptimal);

			for (size_t i = 0; i < pixels.size(); ++i)
			{
				if (!CreateImageStagingBuffer(allocator, device, commandBuffer, renderImage.get(), static_cast<uint32_t>(i),
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

			renderImage->TransitionImageLayout(device, commandBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);

			++imageCount;

			image.reset();
		}

		return true;
	}

	bool VulkanSceneManager::SetupMeshInfoBuffer(const Device& device, const vk::CommandBuffer& commandBuffer,
		ChunkData* chunkData, std::vector<std::unique_ptr<Buffer>>& temporaryBuffers, VmaAllocator allocator)
	{
		if (chunkData != nullptr && chunkData->LoadedFromDisk())
		{
			ChunkMemoryEntry entry;
			if (!chunkData->GetGenericData(1, entry))
			{
				return false;
			}

			std::vector<uint8_t> decompressBuffer;
			chunkData->Decompress(entry, decompressBuffer);

			m_meshInfoBuffer = std::make_unique<Buffer>(allocator);
			Buffer* buffer = m_meshInfoBuffer.get();

			bool initialised = buffer->Initialise(decompressBuffer.size(),
				vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer,
				VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
				0,
				vk::SharingMode::eExclusive);

			if (!initialised)
			{
				return false;
			}

			if (!CreateStagingBuffer(allocator, device, commandBuffer, buffer, decompressBuffer.data(),
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
			chunkData->SetGenericData(1, uniformBufferData);

		m_meshInfoBuffer = std::make_unique<Buffer>(allocator);
		Buffer* buffer = m_meshInfoBuffer.get();

		bool initialised = buffer->Initialise(totalSize,
			vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer,
			VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
			0,
			vk::SharingMode::eExclusive);

		if (!initialised)
		{
			return false;
		}

		if (!CreateStagingBuffer(allocator, device, commandBuffer, buffer, uniformBufferData.data(),
			totalSize, temporaryBuffers))
			return false;

		return true;
	}

	bool VulkanSceneManager::Build(ChunkData* chunkData, AsyncData& asyncData)
	{
		const std::lock_guard<std::mutex> lock(m_creationMutex);

		// TODO: Handle resizing
		if (m_indexBuffer.get() != nullptr)
		{
			Logger::Error("Rebuilding existing scene render data currently not supported.");
			asyncData.State = AsyncState::Failed;
			return false;
		}

		auto startTime = std::chrono::high_resolution_clock::now();

		return m_renderer.SubmitResourceCommand([this, chunkData, &asyncData](const vk::CommandBuffer& commandBuffer, std::vector<std::unique_ptr<Buffer>>& temporaryBuffers)
			{
				const Device& device = m_renderer.GetDevice();
				const SwapChain& swapChain = m_renderer.GetSwapChain();
				uint32_t concurrentFrames = m_renderer.GetConcurrentFrameCount();
				VmaAllocator allocator = m_renderer.GetAllocator();
				PhysicalDevice physicalDevice = m_renderer.GetPhysicalDevice();
				const std::vector<std::unique_ptr<Buffer>>& frameInfoBuffers = m_renderer.GetFrameInfoBuffers();

				uint32_t imageCount;
				if (!SetupVertexBuffers(device, commandBuffer, chunkData, temporaryBuffers, allocator))
				{
					return false;
				}

				asyncData.AddSubProgress(50.0f);

				if (!SetupIndexBuffer(device, commandBuffer, chunkData, temporaryBuffers, allocator))
				{
					return false;
				}

				asyncData.AddSubProgress(50.0f);

				if (!SetupRenderImage(&asyncData, device, physicalDevice, commandBuffer, chunkData, temporaryBuffers, allocator, physicalDevice.GetLimits().maxSamplerAnisotropy, imageCount)
					|| !SetupMeshInfoBuffer(device, commandBuffer, chunkData, temporaryBuffers, allocator)
					|| !SetupIndirectDrawBuffer(device, commandBuffer, chunkData, temporaryBuffers, allocator))
				{
					if (asyncData.State != AsyncState::Cancelled)
						asyncData.State = AsyncState::Failed;
					return false;
				}

				if (asyncData.State == AsyncState::Cancelled)
				{
					return false;
				}

				PipelineLayout* pipelineLayout = static_cast<PipelineLayout*>(m_shader);
				if (!pipelineLayout->Rebuild(device, swapChain, imageCount))
				{
					asyncData.State = AsyncState::Failed;
					return false;
				}

				std::vector<vk::DescriptorSetLayout> layouts;
				for (uint32_t i = 0; i < concurrentFrames; ++i)
				{
					layouts.append_range(pipelineLayout->GetDescriptorSetLayouts());
				}
				m_descriptorSets = m_descriptorPool->CreateDescriptorSets(device, layouts);

				std::array<vk::DescriptorImageInfo, 1> samplerInfos =
				{
					vk::DescriptorImageInfo(m_sampler->Get(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal)
				};

				std::vector<vk::DescriptorImageInfo> imageInfos;
				imageInfos.resize(m_imageArrayView.size());
				for (uint32_t i = 0; i < m_imageArrayView.size(); ++i)
				{
					imageInfos[i] = vk::DescriptorImageInfo(nullptr, m_imageArrayView[i]->Get(), vk::ImageLayout::eShaderReadOnlyOptimal);
				}

				std::array<vk::DescriptorBufferInfo, 1> instanceBufferInfos =
				{
					vk::DescriptorBufferInfo(m_meshInfoBuffer->Get(), 0, sizeof(RenderMeshInfo) * m_meshInfos.size())
				};

				for (uint32_t i = 0; i < concurrentFrames; ++i)
				{
					std::array<vk::DescriptorBufferInfo, 1> frameBufferInfos =
					{
						vk::DescriptorBufferInfo(frameInfoBuffers[i]->Get(), 0, sizeof(FrameInfoUniformBuffer))
					};

					std::array<vk::WriteDescriptorSet, 4> writeDescriptorSets =
					{
						vk::WriteDescriptorSet(m_descriptorSets[i], 0, 0, vk::DescriptorType::eUniformBuffer, nullptr, frameBufferInfos),
						vk::WriteDescriptorSet(m_descriptorSets[i], 1, 0, vk::DescriptorType::eStorageBuffer, nullptr, instanceBufferInfos),
						vk::WriteDescriptorSet(m_descriptorSets[i], 2, 0, vk::DescriptorType::eSampler, samplerInfos),
						vk::WriteDescriptorSet(m_descriptorSets[i], 3, 0, vk::DescriptorType::eSampledImage, imageInfos)
					};

					device.Get().updateDescriptorSets(writeDescriptorSets, nullptr);
				}

				return true;
			}, [startTime]()
				{
					auto endTime = std::chrono::high_resolution_clock::now();
					float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(endTime - startTime).count();
					Logger::Verbose("Scene manager build finished in {} seconds.", deltaTime);
				});
	}

	void VulkanSceneManager::Draw(const vk::CommandBuffer& commandBuffer, uint32_t currentFrameIndex)
	{
		if (m_vertexBuffers.empty() || m_creating)
			return;

		const PipelineLayout* pipelineLayout = static_cast<const PipelineLayout*>(m_shader);
		const vk::Pipeline& graphicsPipeline = pipelineLayout->GetGraphicsPipeline();
		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);

		uint32_t vertexBufferCount = static_cast<uint32_t>(m_vertexBuffers.size());

		std::vector<vk::DeviceSize> vertexBufferOffsets;
		vertexBufferOffsets.resize(vertexBufferCount);
		std::vector<vk::Buffer> vertexBufferViews;
		vertexBufferViews.reserve(vertexBufferCount);
		for (const auto& buffer : m_vertexBuffers)
		{
			vertexBufferViews.emplace_back(buffer->Get());
		}

		commandBuffer.bindVertexBuffers(0, vertexBufferViews, vertexBufferOffsets);
		commandBuffer.bindIndexBuffer(m_indexBuffer->Get(), 0, vk::IndexType::eUint32);
		commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout->Get(), 0, m_descriptorSets[currentFrameIndex], nullptr);

		uint32_t drawCount = m_meshCapacity; // TODO: Compute counted, after culling, etc.
		commandBuffer.drawIndexedIndirect(m_indirectDrawBuffer->Get(), 0, drawCount, sizeof(vk::DrawIndexedIndirectCommand));
	}
}