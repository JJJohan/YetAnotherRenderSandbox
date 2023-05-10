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
#include "CommandPool.hpp"
#include "FrameInfoUniformBuffer.hpp"
#include "RenderMeshInfo.hpp"
#include "PipelineLayout.hpp"
#include "Core/Logging/Logger.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vma/vk_mem_alloc.h>

using namespace Engine::Logging;

namespace Engine::Rendering::Vulkan
{
	VulkanSceneManager::VulkanSceneManager(const uint32_t maxConcurrentFrames)
		: m_maxConcurrentFrames(maxConcurrentFrames)
		, m_resourceFence()
		, m_prevFrameCommandBuffers()
		, m_prevFrameBuffers()
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

	bool VulkanSceneManager::Initialise(VmaAllocator allocator, const Device& device, const vk::CommandBuffer& setupCommandBuffer,
		Shader* shader, const vk::PhysicalDeviceLimits& limits)
	{
		m_shader = shader;
		m_resourceFence = device.Get().createFenceUnique(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));

		m_sampler = std::make_unique<ImageSampler>();
		bool samplerInitialised = m_sampler->Initialise(device, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear, vk::SamplerAddressMode::eRepeat, limits.maxSamplerAnisotropy);
		if (!samplerInitialised)
		{
			return false;
		}

		m_blankImage = std::make_shared<RenderImage>(allocator);
		bool imageInitialised = m_blankImage->Initialise(vk::ImageType::e2D, vk::Format::eR8G8B8A8Srgb, vk::Extent3D(1, 1, 1), vk::SampleCountFlagBits::e1, false, vk::ImageTiling::eOptimal,
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

		m_blankImage->TransitionImageLayout(device, setupCommandBuffer, vk::ImageLayout::eTransferDstOptimal);

		Colour blankPixel = Colour();
		if (!CreateImageStagingBuffer(allocator, device, setupCommandBuffer, m_blankImage.get(), &blankPixel,
			sizeof(uint32_t), m_prevFrameBuffers))
			return false;

		m_blankImage->TransitionImageLayout(device, setupCommandBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);

		std::vector<vk::DescriptorPoolSize> poolSizes =
		{
			vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, m_maxConcurrentFrames),
			vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, m_maxConcurrentFrames),
			vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, m_maxConcurrentFrames),
		};

		m_descriptorPool = std::make_unique<DescriptorPool>();
		if (!m_descriptorPool->Initialise(device, m_maxConcurrentFrames, poolSizes))
		{
			return false;
		}

		return true;
	}

	bool VulkanSceneManager::SetupIndirectDrawBuffer(const Device& device, const vk::CommandBuffer& commandBuffer,
		std::vector<std::unique_ptr<Buffer>>& temporaryBuffers, VmaAllocator allocator)
	{
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

		return true;
	}

	bool VulkanSceneManager::SetupVertexBuffers(const Device& device, const vk::CommandBuffer& commandBuffer,
		std::vector<std::unique_ptr<Buffer>>& temporaryBuffers, VmaAllocator allocator)
	{
		// Dodgy...
		m_vertexBuffers.resize(2);
		m_vertexOffsets.resize(m_vertexDataArrays.size());

		for (uint32_t vertexBit = 0; vertexBit < 2; ++vertexBit)
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
		std::vector<std::unique_ptr<Buffer>>& temporaryBuffers, VmaAllocator allocator)
	{
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
		const vk::CommandBuffer& commandBuffer, const RenderImage* destinationImage, const void* data, uint64_t size,
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

		stagingBuffer->CopyToImage(device, commandBuffer, *destinationImage);

		return true;
	}

	bool VulkanSceneManager::SetupRenderImage(const Device& device,
		const vk::CommandBuffer& commandBuffer, std::vector<std::unique_ptr<Buffer>>& temporaryBuffers,
		VmaAllocator allocator, float maxAnisotropy, uint32_t& imageCount)
	{
		m_imageArray.reserve(m_images.size());
		m_imageArrayView.reserve(m_images.size());
		imageCount = 0;

		for (size_t i = 0; i < m_images.size(); ++i)
		{
			if (!m_active[i])
				continue;

			std::shared_ptr<Image>& image = m_images[i];
			if (image.get() == nullptr)
			{
				continue;
			}

			uint32_t componentCount = image->GetComponentCount();
			vk::Format format;
			if (componentCount == 4)
			{
				format = vk::Format::eR8G8B8A8Srgb;
			}
			else if (componentCount == 3)
			{
				format = vk::Format::eR8G8B8Srgb;
			}
			else
			{
				Logger::Error("Images without exactly 3 or 4 channels are currently not supported.");
				return false;
			}

			const glm::uvec2& size = image->GetSize();
			vk::Extent3D dimensions(size.x, size.y, 1);

			std::unique_ptr<RenderImage>& renderImage = m_imageArray.emplace_back(std::make_unique<RenderImage>(allocator));
			bool imageInitialised = renderImage->Initialise(vk::ImageType::e2D, format, dimensions, vk::SampleCountFlagBits::e1, true, vk::ImageTiling::eOptimal,
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

			const std::vector<uint8_t>& pixels = image->GetPixels();

			renderImage->TransitionImageLayout(device, commandBuffer, vk::ImageLayout::eTransferDstOptimal);

			if (!CreateImageStagingBuffer(allocator, device, commandBuffer, renderImage.get(), pixels.data(),
				pixels.size(), temporaryBuffers))
				return false;

			renderImage->GenerateMipmaps(device, commandBuffer);

			++imageCount;

			image.reset();
		}

		return true;
	}

	bool VulkanSceneManager::SetupMeshInfoBuffer(const Device& device, const vk::CommandBuffer& commandBuffer,
		std::vector<std::unique_ptr<Buffer>>& temporaryBuffers, VmaAllocator allocator, vk::DeviceSize minOffsetAlignment)
	{
		uint64_t totalSize = 0;
		for (uint32_t i = 0; i < m_meshCapacity; ++i)
		{
			if (!m_active[i])
				continue;

			totalSize += sizeof(MeshInfo);
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
			data.colour = meshInfo.colour.GetVec4();
			data.imageIndex = static_cast<uint32_t>(meshInfo.imageIndex);
			memcpy(uniformBufferData.data() + totalSize, &data, sizeof(MeshInfo));
			totalSize += sizeof(MeshInfo);
		}

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

	bool VulkanSceneManager::Update(VmaAllocator allocator, const Device& device, const CommandPool& resourceCommandPool,
		const std::vector<std::unique_ptr<Buffer>>& frameInfoBuffers, const RenderPass& renderPass, const vk::PhysicalDeviceLimits& limits)
	{
		const std::lock_guard<std::mutex> lock(m_creationMutex);
		const vk::Device& deviceImp = device.Get();

		if (!m_prevFrameBuffers.empty() || !m_prevFrameCommandBuffers.empty())
		{
			if (deviceImp.waitForFences(1, &m_resourceFence.get(), true, UINT64_MAX) != vk::Result::eSuccess)
			{
				return false;
			}

			m_prevFrameBuffers.clear();
			m_prevFrameCommandBuffers.clear();

			if (deviceImp.resetFences(1, &m_resourceFence.get()) != vk::Result::eSuccess)
			{
				return false;
			}
		}

		if (!m_build)
			return true;

		// TODO: Handle resizing
		if (m_indexBuffer.get() != nullptr)
		{
			Logger::Error("Rebuilding existing scene render data currently not supported.");
			return false;
		}

		m_build = false;

		vk::UniqueCommandBuffer commandBuffer = resourceCommandPool.BeginResourceCommandBuffer(device);
		std::vector<std::unique_ptr<Buffer>> temporaryBuffers;

		uint32_t imageCount;
		if (!SetupVertexBuffers(device, commandBuffer.get(), temporaryBuffers, allocator)
			|| !SetupIndexBuffer(device, commandBuffer.get(), temporaryBuffers, allocator)
			|| !SetupRenderImage(device, commandBuffer.get(), temporaryBuffers, allocator, limits.maxSamplerAnisotropy, imageCount)
			|| !SetupMeshInfoBuffer(device, commandBuffer.get(), temporaryBuffers, allocator, limits.minUniformBufferOffsetAlignment)
			|| !SetupIndirectDrawBuffer(device, commandBuffer.get(), temporaryBuffers, allocator))
		{
			return false;
		}

		PipelineLayout* pipelineLayout = static_cast<PipelineLayout*>(m_shader);
		if (!pipelineLayout->Rebuild(device, renderPass, imageCount))
		{
			return false;
		}

		std::vector<vk::DescriptorSetLayout> layouts;
		for (uint32_t i = 0; i < m_maxConcurrentFrames; ++i)
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
			vk::DescriptorBufferInfo(m_meshInfoBuffer->Get(), 0, sizeof(MeshInfo) * m_meshInfos.size())
		};

		for (uint32_t i = 0; i < m_maxConcurrentFrames; ++i)
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

		commandBuffer->end();
		const vk::Queue& queue = device.GetGraphicsQueue();

		vk::SubmitInfo submitInfo;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer.get();

		vk::Result submitResult = queue.submit(1, &submitInfo, m_resourceFence.get());
		if (submitResult != vk::Result::eSuccess)
		{
			return false;
		}

		m_prevFrameCommandBuffers.push_back(std::move(commandBuffer));

		for (auto& buffer : temporaryBuffers)
		{
			m_prevFrameBuffers.push_back(std::move(buffer));
		}

		return true;
	}

	void VulkanSceneManager::Draw(const vk::CommandBuffer& commandBuffer, uint32_t currentFrameIndex)
	{
		if (m_vertexBuffers.empty())
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