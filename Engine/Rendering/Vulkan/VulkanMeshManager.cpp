#include "VulkanMeshManager.hpp"
#include "PhysicalDevice.hpp"
#include "Device.hpp"
#include "CommandBuffer.hpp"
#include "CommandPool.hpp"
#include "PipelineLayout.hpp"
#include "Core/Logging/Logger.hpp"
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace Engine::Logging;

namespace Engine::Rendering::Vulkan
{
	VulkanMeshManager::VulkanMeshManager(const uint32_t maxConcurrentFrames)
		: m_maxConcurrentFrames(maxConcurrentFrames)
	{
	}

	void VulkanMeshManager::IncrementSize()
	{
		MeshManager::IncrementSize();

		m_vertexCounts.push_back(0);
		m_indexCounts.push_back(0);
		m_positionBuffers.push_back(nullptr);
		m_colourBuffers.push_back(nullptr);
		m_indexBuffers.push_back(nullptr);
		m_uniformBufferArrays.push_back({});
		m_uniformBuffersMappedArrays.push_back({});
		m_descriptorPools.push_back(nullptr);
		m_descriptorSetArrays.push_back({});
		m_pipelineLayouts.push_back(nullptr);
	}

	uint32_t VulkanMeshManager::CreateMesh(const Shader* shader,
		const std::vector<glm::vec3>& positions,
		const std::vector<Colour>& vertexColours,
		const std::vector<uint32_t>& indices,
		const Colour& colour,
		const glm::mat4& transform)
	{
		const std::lock_guard<std::mutex> lock(m_creationMutex);

		uint32_t id = MeshManager::CreateMesh(shader, positions, vertexColours, indices, colour, transform);

		const PipelineLayout* pipelineLayout = static_cast<const PipelineLayout*>(shader);
		m_pipelineLayouts[id] = pipelineLayout;

		return id;
	}

	void VulkanMeshManager::DestroyMesh(uint32_t id)
	{
		const std::lock_guard<std::mutex> lock(m_creationMutex);

		// Reset versions?

		MeshManager::DestroyMesh(id);
	}

	bool VulkanMeshManager::SetupPositionBuffer(const PhysicalDevice& physicalDevice, const Device& device, uint32_t id)
	{
		const std::vector<glm::vec3>& positions = m_positionArrays[id];
		m_vertexCounts[id] = static_cast<uint32_t>(positions.size());
		uint64_t positionsSize = static_cast<uint64_t>(sizeof(glm::vec3) * m_vertexCounts[id]);

		m_positionBuffers[id] = std::make_unique<Buffer>();
		return m_positionBuffers[id]->Initialise(physicalDevice, device, positionsSize,
			vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			vk::SharingMode::eExclusive);
	}

	bool VulkanMeshManager::SetupColourBuffer(const PhysicalDevice& physicalDevice, const Device& device, uint32_t id)
	{
		const std::vector<Colour>& vertexColours = m_vertexColourArrays[id];
		uint64_t coloursSize = static_cast<uint64_t>(sizeof(uint32_t) * vertexColours.size());

		m_colourBuffers[id] = std::make_unique<Buffer>();
		return m_colourBuffers[id]->Initialise(physicalDevice, device, coloursSize,
			vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			vk::SharingMode::eExclusive);
	}

	bool VulkanMeshManager::SetupIndexBuffer(const PhysicalDevice& physicalDevice, const Device& device, uint32_t id)
	{
		const std::vector<uint32_t>& indices = m_indexArrays[id];
		m_indexCounts[id] = static_cast<uint32_t>(indices.size());
		uint64_t indicesSize = static_cast<uint64_t>(sizeof(uint32_t) * m_indexCounts[id]);

		m_indexBuffers[id] = std::make_unique<Buffer>();
		return m_indexBuffers[id]->Initialise(physicalDevice, device, indicesSize,
			vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			vk::SharingMode::eExclusive);
	}

	bool VulkanMeshManager::CreateStagingBuffer(const PhysicalDevice& physicalDevice, const Device& device,
		const CommandPool& resourceCommandPool, const Buffer& destinationBuffer, const void* data, uint64_t size,
		std::vector<std::unique_ptr<Buffer>>& copyBufferCollection, std::vector<vk::UniqueCommandBuffer>& copyCommandCollection)
	{
		Buffer* stagingBuffer = copyBufferCollection.emplace_back(std::make_unique<Buffer>()).get();
		if (!stagingBuffer->Initialise(physicalDevice, device, size,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			vk::SharingMode::eExclusive))
		{
			return false;
		}


		if (!stagingBuffer->UpdateContents(device, 0, data, size))
			return false;

		copyCommandCollection.emplace_back(stagingBuffer->Copy(device, resourceCommandPool, destinationBuffer, size));

		return true;
	}

	// hard-coded for now
	struct UniformBufferObject
	{
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 proj;
	};

	bool VulkanMeshManager::SetupUniformBuffers(const PhysicalDevice& physicalDevice, const Device& device, uint32_t id)
	{
		const vk::Device& deviceImp = device.Get();
		vk::DeviceSize bufferSize = sizeof(UniformBufferObject);

		m_uniformBufferArrays[id].resize(m_maxConcurrentFrames);
		m_uniformBuffersMappedArrays[id].resize(m_maxConcurrentFrames);
		std::vector<std::unique_ptr<Buffer>>& uniformBuffers = m_uniformBufferArrays[id];
		std::vector<void*>& mappedBuffers = m_uniformBuffersMappedArrays[id];

		const glm::mat4 model = m_transforms[id];

		for (uint32_t i = 0; i < m_maxConcurrentFrames; ++i)
		{
			uniformBuffers[i] = std::make_unique<Buffer>();
			Buffer& buffer = *uniformBuffers[i];

			if (!buffer.Initialise(physicalDevice, device, bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, vk::SharingMode::eExclusive))
			{
				return false;
			}

			mappedBuffers[i] = deviceImp.mapMemory(buffer.GetMemory(), 0, bufferSize);
			memcpy(mappedBuffers[i], &model, sizeof(glm::mat4)); // Only copy model matrix
		}

		return true;
	}

	bool VulkanMeshManager::CreateMeshResources(const PhysicalDevice& physicalDevice, const Device& device, uint32_t id)
	{
		if (!SetupPositionBuffer(physicalDevice, device, id)
			|| !SetupColourBuffer(physicalDevice, device, id)
			|| !SetupIndexBuffer(physicalDevice, device, id)
			|| !SetupUniformBuffers(physicalDevice, device, id))
		{
			return false;
		}

		vk::DescriptorPoolSize poolSize(vk::DescriptorType::eUniformBuffer, m_maxConcurrentFrames);
		m_descriptorPools[id] = std::make_unique<DescriptorPool>();
		if (!m_descriptorPools[id]->Initialise(device, m_maxConcurrentFrames, { poolSize }))
		{
			return false;
		}

		std::vector<vk::DescriptorSetLayout> layouts;
		for (uint32_t i = 0; i < m_maxConcurrentFrames; ++i)
		{
			layouts.append_range(m_pipelineLayouts[id]->GetDescriptorSetLayouts());
		}

		m_descriptorSetArrays[id] = m_descriptorPools[id]->CreateDescriptorSets(device, layouts);

		const std::vector<std::unique_ptr<Buffer>>& uniformBuffers = m_uniformBufferArrays[id];
		const std::vector<vk::DescriptorSet>& descriptorSets = m_descriptorSetArrays[id];
		for (uint32_t i = 0; i < m_maxConcurrentFrames; ++i)
		{
			vk::DescriptorBufferInfo bufferInfo(uniformBuffers[i]->Get(), 0, sizeof(UniformBufferObject));
			std::array<vk::DescriptorBufferInfo, 1> bufferInfoArray = { bufferInfo };
			vk::WriteDescriptorSet descriptorWrite(descriptorSets[i], 0, 0, vk::DescriptorType::eUniformBuffer, nullptr, bufferInfoArray);
			device.Get().updateDescriptorSets(1, &descriptorWrite, 0, nullptr);
		}

		m_updateFlags[id] = MeshUpdateFlagBits::All;
		return true;
	}

	bool VulkanMeshManager::Update(const PhysicalDevice& physicalDevice, const Device& device, const CommandPool& resourceCommandPool)
	{
		const std::lock_guard<std::mutex> lock(m_creationMutex);

		const vk::Device& deviceImp = device.Get();

		std::vector<vk::UniqueCommandBuffer> copyCommands;
		std::vector<std::unique_ptr<Buffer>> temporaryBuffers;
		std::vector<uint32_t> toDelete;

		uint32_t meshCount = static_cast<uint32_t>(m_vertexCounts.size());
		for (uint32_t id = 0; id < meshCount; ++id)
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
				if (!CreateMeshResources(physicalDevice, device, id))
					return false;
			}

			MeshUpdateFlagBits updateBits = m_updateFlags[id];
			if (updateBits == MeshUpdateFlagBits::None)
				continue;

			if ((updateBits & MeshUpdateFlagBits::Positions) == MeshUpdateFlagBits::Positions)
			{
				if (!CreateStagingBuffer(physicalDevice, device, resourceCommandPool, *m_positionBuffers[id], m_positionArrays[id].data(),
					static_cast<uint64_t>(m_positionArrays[id].size()) * sizeof(glm::vec3), temporaryBuffers, copyCommands))
					return false;
			}

			if ((updateBits & MeshUpdateFlagBits::VertexColours) == MeshUpdateFlagBits::VertexColours)
			{
				if (!CreateStagingBuffer(physicalDevice, device, resourceCommandPool, *m_colourBuffers[id], m_vertexColourArrays[id].data(),
					static_cast<uint64_t>(m_vertexColourArrays[id].size()) * sizeof(Colour), temporaryBuffers, copyCommands))
					return false;
			}

			if ((updateBits & MeshUpdateFlagBits::Indices) == MeshUpdateFlagBits::Indices)
			{
				if (!CreateStagingBuffer(physicalDevice, device, resourceCommandPool, *m_indexBuffers[id], m_indexArrays[id].data(),
					static_cast<uint64_t>(m_indexArrays[id].size()) * sizeof(uint32_t), temporaryBuffers, copyCommands))
					return false;
			}

			if ((updateBits & MeshUpdateFlagBits::Colour) == MeshUpdateFlagBits::Colour)
			{
				// TODO
			}

			if ((updateBits & MeshUpdateFlagBits::Transform) == MeshUpdateFlagBits::Transform)
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
				m_positionBuffers[id].release();
				m_colourBuffers[id].release();
				m_indexBuffers[id].release();
				m_uniformBufferArrays[id].clear();
				m_uniformBuffersMappedArrays[id].clear();
				m_descriptorPools[id].release();
				m_descriptorSetArrays[id].clear();
			}
		}

		uint32_t copyCommandCount = static_cast<uint32_t>(copyCommands.size());
		if (copyCommandCount == 0)
		{
			return true;
		}

		std::vector<vk::CommandBuffer> copyCommandViews;
		copyCommandViews.resize(copyCommandCount);
		for (uint32_t i = 0; i < copyCommandCount; ++i)
		{
			copyCommandViews[i] = copyCommands[i].get();
		}

		vk::SubmitInfo submitInfo;
		submitInfo.commandBufferCount = copyCommandCount;
		submitInfo.pCommandBuffers = copyCommandViews.data();

		vk::Result submitResult = queue.submit(1, &submitInfo, nullptr);
		if (submitResult != vk::Result::eSuccess)
		{
			return false;
		}

		queue.waitIdle(); // boo

		return true;
	}

	void VulkanMeshManager::Draw(const vk::CommandBuffer& commandBuffer, const vk::Extent2D& viewSize, uint32_t currentFrameIndex)
	{
		const std::lock_guard<std::mutex> lock(m_creationMutex); // Ideally have a set of data for 'next' frame so no locking?

		uint32_t meshCount = static_cast<uint32_t>(m_vertexCounts.size());
		for (uint32_t id = 0; id < meshCount; ++id)
		{
			if (!m_active[id])
				continue;

			// Update uniform buffers

			UniformBufferObject ubo;
			ubo.view = glm::lookAt(glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
			ubo.proj = glm::perspective(glm::radians(75.0f), viewSize.width / (float)viewSize.height, 0.1f, 10.0f);
			ubo.proj[1][1] *= -1;

			std::vector<void*>& mappedBuffers = m_uniformBuffersMappedArrays[id];
			memcpy((uint8_t*)mappedBuffers[currentFrameIndex] + sizeof(glm::mat4), &ubo.view, sizeof(glm::mat4) * 2);

			// Draw stuff

			const vk::Pipeline& graphicsPipeline = m_pipelineLayouts[id]->GetGraphicsPipeline();
			commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);

			std::array<vk::Buffer, 2> vertexBuffers = { m_positionBuffers[id]->Get(), m_colourBuffers[id]->Get() };
			std::array<vk::DeviceSize, 2> offsets = { 0, 0 };
			commandBuffer.bindVertexBuffers(0, vertexBuffers, offsets);

			vk::DeviceSize indexOffset = 0;
			const vk::Buffer& indexBuffer = m_indexBuffers[id]->Get();
			commandBuffer.bindIndexBuffer(indexBuffer, indexOffset, vk::IndexType::eUint32);

			const std::vector<vk::DescriptorSet>& descriptorSets = m_descriptorSetArrays[id];
			commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayouts[id]->Get(), 0, descriptorSets[currentFrameIndex], nullptr);

			commandBuffer.drawIndexed(m_indexCounts[id], 1, 0, 0, 0);
		}
	}
}