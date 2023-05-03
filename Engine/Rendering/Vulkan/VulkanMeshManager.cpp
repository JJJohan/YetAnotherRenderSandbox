#include "VulkanMeshManager.hpp"
#include "PhysicalDevice.hpp"
#include "Device.hpp"
#include "CommandBuffer.hpp"
#include "DescriptorPool.hpp"
#include "Buffer.hpp"
#include "RenderImage.hpp"
#include "ImageView.hpp"
#include "ImageSampler.hpp"
#include "CommandPool.hpp"
#include "PipelineLayout.hpp"
#include "Rendering/Camera.hpp"
#include "Core/Logging/Logger.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vma/vk_mem_alloc.h>

using namespace Engine::Logging;

namespace Engine::Rendering::Vulkan
{
	VulkanMeshManager::VulkanMeshManager(const uint32_t maxConcurrentFrames)
		: m_maxConcurrentFrames(maxConcurrentFrames)
		, m_vertexCounts()
		, m_indexCounts()
		, m_vertexBuffers()
		, m_indexBuffers()
		, m_renderImages()
		, m_renderImageViews()
		, m_sampler(nullptr)
		, m_uniformBufferArrays()
		, m_uniformBuffersMappedArrays()
		, m_descriptorPools()
		, m_descriptorSetArrays()
	{
	}

	void VulkanMeshManager::IncrementSize()
	{
		MeshManager::IncrementSize();

		m_vertexCounts.push_back(0);
		m_indexCounts.push_back(0);
		m_vertexBuffers.push_back({});
		m_indexBuffers.push_back(nullptr);
		m_uniformBufferArrays.push_back({});
		m_uniformBuffersMappedArrays.push_back({});
		m_descriptorPools.push_back(nullptr);
		m_descriptorSetArrays.push_back({});
		m_renderImages.push_back(nullptr);
		m_renderImageViews.push_back(nullptr);
	}

	void VulkanMeshManager::DestroyMesh(uint32_t id)
	{
		const std::lock_guard<std::mutex> lock(m_creationMutex);

		MeshManager::DestroyMesh(id);
	}

	bool VulkanMeshManager::SetupVertexBuffers(VmaAllocator allocator, uint32_t id)
	{
		const std::vector<VertexData>& vertexData = m_vertexDataArrays[id];
		uint32_t vertexBufferCount = static_cast<uint32_t>(vertexData.size());
		if (vertexBufferCount == 0)
		{
			Logger::Error("No vertex buffers are requested which is not supported.");
			return false;
		}

		m_vertexCounts[id] = vertexData[0].GetCount();
		m_vertexBuffers[id].reserve(vertexBufferCount);

		uint32_t index = 0;
		for (const auto& data : vertexData)
		{
			uint32_t elementCount = data.GetCount();
			if (elementCount != m_vertexCounts[id])
			{
				Logger::Error("Vertex count mismatch in provided vertex data.");
				return false;
			}

			uint64_t bufferSize = static_cast<uint64_t>(data.GetElementSize()) * elementCount;

			std::unique_ptr<Buffer> buffer = std::make_unique<Buffer>(allocator);
			bool initialised = buffer->Initialise(bufferSize,
				vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
				VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
				0,
				vk::SharingMode::eExclusive);

			if (!initialised)
			{
				return false;
			}

			m_vertexUpdateFlags[id] |= 1 << index;
			m_vertexBuffers[id].push_back(std::move(buffer));
			++index;
		}

		return true;
	}

	bool VulkanMeshManager::SetupIndexBuffer(VmaAllocator allocator, uint32_t id)
	{
		const std::vector<uint32_t>& indices = m_indexArrays[id];
		m_indexCounts[id] = static_cast<uint32_t>(indices.size());
		uint64_t indicesSize = static_cast<uint64_t>(sizeof(uint32_t) * m_indexCounts[id]);

		m_indexBuffers[id] = std::make_unique<Buffer>(allocator);
		return m_indexBuffers[id]->Initialise(indicesSize,
			vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
			VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
			0,
			vk::SharingMode::eExclusive);
	}

	bool VulkanMeshManager::CreateStagingBuffer(VmaAllocator allocator, const Device& device,
		const CommandPool& resourceCommandPool, const Buffer* destinationBuffer, const void* data, uint64_t size,
		std::vector<std::unique_ptr<Buffer>>& copyBufferCollection, std::vector<vk::UniqueCommandBuffer>& resourceCommands)
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

		resourceCommands.emplace_back(stagingBuffer->Copy(device, resourceCommandPool, *destinationBuffer, size));

		return true;
	}

	bool VulkanMeshManager::CreateImageStagingBuffer(VmaAllocator allocator, const Device& device,
		const CommandPool& resourceCommandPool, const RenderImage* destinationImage, const void* data, uint64_t size,
		std::vector<std::unique_ptr<Buffer>>& copyBufferCollection, std::vector<vk::UniqueCommandBuffer>& resourceCommands)
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

		resourceCommands.emplace_back(stagingBuffer->CopyToImage(device, resourceCommandPool, *destinationImage));

		return true;
	}

	// hard-coded for now
	struct UniformBufferObject
	{
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 proj;
		glm::vec4 colour;
	};

	bool VulkanMeshManager::SetupUniformBuffers(VmaAllocator allocator, uint32_t id)
	{
		vk::DeviceSize bufferSize = sizeof(UniformBufferObject);

		m_uniformBufferArrays[id].resize(m_maxConcurrentFrames);
		m_uniformBuffersMappedArrays[id].resize(m_maxConcurrentFrames);
		std::vector<std::unique_ptr<Buffer>>& uniformBuffers = m_uniformBufferArrays[id];
		std::vector<void*>& mappedBuffers = m_uniformBuffersMappedArrays[id];

		const glm::mat4 model = m_transforms[id];

		for (uint32_t i = 0; i < m_maxConcurrentFrames; ++i)
		{
			uniformBuffers[i] = std::make_unique<Buffer>(allocator);
			Buffer& buffer = *uniformBuffers[i];

			if (!buffer.Initialise(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, VMA_MEMORY_USAGE_AUTO,
				VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, vk::SharingMode::eExclusive))
			{
				return false;
			}

			if (!buffer.GetMappedMemory(&mappedBuffers[i]))
			{
				return false;
			}

			memcpy(static_cast<uint8_t*>(mappedBuffers[i]), &model, sizeof(glm::mat4));

			// temporary - wasting space right now.
			glm::vec4 colour(static_cast<float>(m_colours[id].R) / 255.0f, 
				static_cast<float>(m_colours[id].G) / 255.0f,
				static_cast<float>(m_colours[id].B) / 255.0f,
				static_cast<float>(m_colours[id].A) / 255.0f);
			memcpy(static_cast<uint8_t*>(mappedBuffers[i]) + sizeof(glm::mat4) * 3, &colour, sizeof(glm::vec4));
		}

		return true;
	}

	bool VulkanMeshManager::SetupRenderImage(VmaAllocator allocator, const Device& device, uint32_t id, float maxAnisotropy)
	{
		const std::shared_ptr<Image>& image = m_images[id];
		if (image.get() == nullptr)
		{
			return true;
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

		m_renderImages[id] = std::make_unique<RenderImage>(allocator);
		bool imageInitialised = m_renderImages[id]->Initialise(vk::ImageType::e2D, format, dimensions, vk::SampleCountFlagBits::e1, true, vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
			VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
			0,
			vk::SharingMode::eExclusive);

		if (!imageInitialised)
		{
			return false;
		}

		m_renderImageViews[id] = std::make_unique<ImageView>();
		bool imageViewInitialised = m_renderImageViews[id]->Initialise(device, m_renderImages[id]->Get(), m_renderImages[id]->GetMiplevels(), m_renderImages[id]->GetFormat(), vk::ImageAspectFlagBits::eColor);
		if (!imageViewInitialised)
		{
			return false;
		}

		return true;
	}

	bool VulkanMeshManager::CreateMeshResources(VmaAllocator allocator, const Device& device, uint32_t id, float maxAnisotropy)
	{
		if (!SetupVertexBuffers(allocator, id)
			|| !SetupIndexBuffer(allocator, id)
			|| !SetupUniformBuffers(allocator, id)
			|| !SetupRenderImage(allocator, device, id, maxAnisotropy))
		{
			return false;
		}

		std::vector<vk::DescriptorPoolSize> poolSizes =
		{
			vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, m_maxConcurrentFrames),
			vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, m_maxConcurrentFrames),
		};

		m_descriptorPools[id] = std::make_unique<DescriptorPool>();
		if (!m_descriptorPools[id]->Initialise(device, m_maxConcurrentFrames, poolSizes))
		{
			return false;
		}

		std::vector<vk::DescriptorSetLayout> layouts;
		const PipelineLayout* pipelineLayout = static_cast<const PipelineLayout*>(m_shaders[id]);
		for (uint32_t i = 0; i < m_maxConcurrentFrames; ++i)
		{
			layouts.append_range(pipelineLayout->GetDescriptorSetLayouts());
		}
		m_descriptorSetArrays[id] = m_descriptorPools[id]->CreateDescriptorSets(device, layouts);

		const std::vector<std::unique_ptr<Buffer>>& uniformBuffers = m_uniformBufferArrays[id];
		const std::unique_ptr<ImageView>& imageView = m_renderImageViews[id].get() != nullptr ? m_renderImageViews[id] : m_blankImageView;
		const std::vector<vk::DescriptorSet>& descriptorSets = m_descriptorSetArrays[id];

		for (uint32_t i = 0; i < m_maxConcurrentFrames; ++i)
		{
			std::array<vk::DescriptorBufferInfo, 1> bufferInfos =
			{
				vk::DescriptorBufferInfo(uniformBuffers[i]->Get(), 0, sizeof(UniformBufferObject))
			};

			std::array<vk::DescriptorImageInfo, 1> imageInfos =
			{
				vk::DescriptorImageInfo(m_sampler->Get(), imageView->Get(), vk::ImageLayout::eShaderReadOnlyOptimal)
			};

			std::array<vk::WriteDescriptorSet, 2> writeDescriptorSets =
			{
				vk::WriteDescriptorSet(descriptorSets[i], 0, 0, vk::DescriptorType::eUniformBuffer, nullptr, bufferInfos),
				vk::WriteDescriptorSet(descriptorSets[i], 1, 0, vk::DescriptorType::eCombinedImageSampler, imageInfos)
			};

			device.Get().updateDescriptorSets(writeDescriptorSets, nullptr);
		}

		m_updateFlags[id] = MeshUpdateFlagBits::All;
		return true;
	}

	bool VulkanMeshManager::Update(VmaAllocator allocator, const Device& device, const CommandPool& resourceCommandPool, float maxAnisotropy)
	{
		const std::lock_guard<std::mutex> lock(m_creationMutex);

		const vk::Device& deviceImp = device.Get();

		std::vector<vk::UniqueCommandBuffer> resourceCommands;
		std::vector<std::unique_ptr<Buffer>> temporaryBuffers;
		std::vector<uint32_t> toDelete;

		if (m_sampler.get() == nullptr)
		{
			m_sampler = std::make_unique<ImageSampler>();
			bool samplerInitialised = m_sampler->Initialise(device, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear, vk::SamplerAddressMode::eRepeat, maxAnisotropy);
			if (!samplerInitialised)
			{
				return false;
			}
		}

		if (m_blankImage.get() == nullptr)
		{
			m_blankImage = std::make_unique<RenderImage>(allocator);
			bool imageInitialised = m_blankImage->Initialise(vk::ImageType::e2D, vk::Format::eR8G8B8A8Srgb, vk::Extent3D(1, 1, 1), vk::SampleCountFlagBits::e1, false, vk::ImageTiling::eOptimal,
				vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
				VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
				0,
				vk::SharingMode::eExclusive);
			if (!imageInitialised)
			{
				return false;
			}

			m_blankImageView = std::make_unique<ImageView>();
			bool imageViewInitialised = m_blankImageView->Initialise(device, m_blankImage->Get(), 1, m_blankImage->GetFormat(), vk::ImageAspectFlagBits::eColor);
			if (!imageViewInitialised)
			{
				return false;
			}

			resourceCommands.emplace_back(m_blankImage->TransitionImageLayout(device, resourceCommandPool, vk::ImageLayout::eTransferDstOptimal));

			Colour blankPixel = Colour();
			if (!CreateImageStagingBuffer(allocator, device, resourceCommandPool, m_blankImage.get(), &blankPixel,
				sizeof(uint32_t), temporaryBuffers, resourceCommands))
				return false;

			resourceCommands.emplace_back(m_blankImage->TransitionImageLayout(device, resourceCommandPool, vk::ImageLayout::eShaderReadOnlyOptimal));
		}

		for (uint32_t id = 0; id < m_meshCapacity; ++id)
		{
			if (!m_active[id])
			{
				if (m_vertexCounts[id] != 0)
				{
					toDelete.push_back(id);
				}
				continue;
			}

			if (m_vertexCounts[id] == 0)
			{
				if (!CreateMeshResources(allocator, device, id, maxAnisotropy))
					return false;
			}

			MeshUpdateFlagBits updateBits = m_updateFlags[id];
			if (updateBits == MeshUpdateFlagBits::None)
				continue;

			if ((updateBits & MeshUpdateFlagBits::VertexData) == MeshUpdateFlagBits::VertexData)
			{
				const std::vector<std::unique_ptr<Buffer>>& vertexBuffers = m_vertexBuffers[id];
				const std::vector<VertexData>& vertexData = m_vertexDataArrays[id];
				uint8_t vertexUpdateBits = m_vertexUpdateFlags[id];
				for (uint32_t vertexBit = 0; vertexBit < 8; ++vertexBit)
				{
					if ((vertexUpdateBits & (1 << vertexBit)) != 0)
					{
						const std::unique_ptr<Buffer>& buffer = vertexBuffers[vertexBit];
						const VertexData& data = vertexData[vertexBit];
						if (!CreateStagingBuffer(allocator, device, resourceCommandPool, buffer.get(), data.GetData(),
							static_cast<uint64_t>(data.GetElementSize()) * data.GetCount(), temporaryBuffers, resourceCommands))
							return false;
					}
				}

				m_vertexUpdateFlags[id] = 0;
			}

			if ((updateBits & MeshUpdateFlagBits::Indices) == MeshUpdateFlagBits::Indices)
			{
				if (!CreateStagingBuffer(allocator, device, resourceCommandPool, m_indexBuffers[id].get(), m_indexArrays[id].data(),
					static_cast<uint64_t>(m_indexArrays[id].size()) * sizeof(uint32_t), temporaryBuffers, resourceCommands))
					return false;
			}

			if ((updateBits & MeshUpdateFlagBits::Image) == MeshUpdateFlagBits::Image)
			{
				if (m_images[id].get() != nullptr)
				{
					const std::vector<uint8_t>& pixels = m_images[id]->GetPixels();

					resourceCommands.emplace_back(m_renderImages[id]->TransitionImageLayout(device, resourceCommandPool, vk::ImageLayout::eTransferDstOptimal));

					if (!CreateImageStagingBuffer(allocator, device, resourceCommandPool, m_renderImages[id].get(), pixels.data(),
						pixels.size(), temporaryBuffers, resourceCommands))
						return false;

					resourceCommands.emplace_back(m_renderImages[id]->GenerateMipmaps(device, resourceCommandPool));

					m_images[id].reset();
				}
			}

			if ((updateBits & MeshUpdateFlagBits::Uniforms) == MeshUpdateFlagBits::Uniforms)
			{
				const glm::mat4 model = m_transforms[id];
				std::vector<void*>& mappedBuffers = m_uniformBuffersMappedArrays[id];
				for (uint32_t i = 0; i < m_maxConcurrentFrames; ++i)
				{
					memcpy(mappedBuffers[i], &model, sizeof(glm::mat4)); // Only copy model matrix
				}
			}

			m_updateFlags[id] = MeshUpdateFlagBits::None;
		}

		const vk::Queue& queue = device.GetGraphicsQueue();

		if (!toDelete.empty())
		{
			queue.waitIdle(); // boo
			for (uint32_t id : toDelete)
			{
				m_vertexCounts[id] = 0;
				m_vertexBuffers[id].clear();
				m_indexBuffers[id].reset();
				m_uniformBufferArrays[id].clear();
				m_uniformBuffersMappedArrays[id].clear();
				m_descriptorPools[id].reset();
				m_descriptorSetArrays[id].clear();
			}
		}

		uint32_t resourceCommandCount = static_cast<uint32_t>(resourceCommands.size());
		if (resourceCommandCount == 0)
		{
			return true;
		}

		std::vector<vk::CommandBuffer> resourceCommandViews;
		resourceCommandViews.resize(resourceCommandCount);
		for (uint32_t i = 0; i < resourceCommandCount; ++i)
		{
			resourceCommandViews[i] = resourceCommands[i].get();
		}

		vk::SubmitInfo submitInfo;
		submitInfo.commandBufferCount = resourceCommandCount;
		submitInfo.pCommandBuffers = resourceCommandViews.data();

		vk::Result submitResult = queue.submit(1, &submitInfo, nullptr);
		if (submitResult != vk::Result::eSuccess)
		{
			return false;
		}

		queue.waitIdle(); // boo

		return true;
	}

	void VulkanMeshManager::Draw(const vk::CommandBuffer& commandBuffer, const Camera& camera, const vk::Extent2D& viewSize, uint32_t currentFrameIndex)
	{
		const std::lock_guard<std::mutex> lock(m_creationMutex); // Ideally have a set of data for 'next' frame so no locking?

		for (uint32_t id = 0; id < m_meshCapacity; ++id)
		{
			if (!m_active[id] || m_vertexCounts[id] == 0)
				continue;

			// Update uniform buffers

			UniformBufferObject ubo{};
			ubo.view = camera.GetView();
			ubo.proj = camera.GetProjection();

			std::vector<void*>& mappedBuffers = m_uniformBuffersMappedArrays[id];
			memcpy((uint8_t*)mappedBuffers[currentFrameIndex] + sizeof(glm::mat4), &ubo.view, sizeof(glm::mat4) * 2);

			// Draw stuff

			const PipelineLayout* pipelineLayout = static_cast<const PipelineLayout*>(m_shaders[id]);
			const vk::Pipeline& graphicsPipeline = pipelineLayout->GetGraphicsPipeline();
			commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);

			const std::vector<std::unique_ptr<Buffer>>& vertexBuffers = m_vertexBuffers[id];
			uint32_t vertexBufferCount = static_cast<uint32_t>(vertexBuffers.size());

			std::vector<vk::DeviceSize> vertexBufferOffsets;
			vertexBufferOffsets.resize(vertexBufferCount);
			std::vector<vk::Buffer> vertexBufferViews;
			vertexBufferViews.reserve(vertexBufferCount);
			for (const auto& buffer : vertexBuffers)
			{
				vertexBufferViews.emplace_back(buffer->Get());
			}

			commandBuffer.bindVertexBuffers(0, vertexBufferViews, vertexBufferOffsets);

			vk::DeviceSize indexOffset = 0;
			const vk::Buffer& indexBuffer = m_indexBuffers[id]->Get();
			commandBuffer.bindIndexBuffer(indexBuffer, indexOffset, vk::IndexType::eUint32);

			const std::vector<vk::DescriptorSet>& descriptorSets = m_descriptorSetArrays[id];
			commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout->Get(), 0, descriptorSets[currentFrameIndex], nullptr);

			commandBuffer.drawIndexed(m_indexCounts[id], 1, 0, 0, 0);
		}
	}
}