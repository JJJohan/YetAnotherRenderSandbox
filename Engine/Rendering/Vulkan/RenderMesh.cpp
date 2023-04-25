#include "RenderMesh.hpp"
#include "PhysicalDevice.hpp"
#include "Device.hpp"
#include "CommandBuffer.hpp"
#include "CommandPool.hpp"
#include "PipelineLayout.hpp"
#include "../Mesh.hpp"
#include "Core/Logging/Logger.hpp"
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace Engine::Logging;

namespace Engine::Rendering::Vulkan
{
	RenderMesh::RenderMesh(const PipelineLayout* pipelineLayout, const uint32_t maxConcurrentFrames)
		: m_positionBuffer(nullptr)
		, m_colourBuffer(nullptr)
		, m_indexBuffer(nullptr)
		, m_descriptorPool(nullptr)
		, m_descriptorSets()
		, m_pipelineLayout(pipelineLayout)
		, m_maxConcurrentFrames(maxConcurrentFrames)
		, m_vertexCount(0)
		, m_indexCount(0)
	{
	}

	std::unique_ptr<Buffer> RenderMesh::SetupPositionBuffer(const PhysicalDevice& physicalDevice, const Device& device, const Mesh& mesh)
	{
		const std::vector<glm::vec3>& positions = mesh.GetPositions();
		m_vertexCount = static_cast<uint32_t>(positions.size());
		uint64_t positionsSize = static_cast<uint64_t>(sizeof(glm::vec3) * m_vertexCount);

		m_positionBuffer = std::make_unique<Buffer>();
		if (!m_positionBuffer->Initialise(physicalDevice, device, positionsSize,
			vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			vk::SharingMode::eExclusive))
		{
			return nullptr;
		}

		std::unique_ptr<Buffer> positionStagingBuffer = std::make_unique<Buffer>();
		if (!positionStagingBuffer->Initialise(physicalDevice, device, positionsSize,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			vk::SharingMode::eExclusive))
		{
			return nullptr;
		}

		positionStagingBuffer->UpdateContents(device, 0, positions.data(), positionsSize);
		return positionStagingBuffer;
	}

	std::unique_ptr<Buffer> RenderMesh::SetupColourBuffer(const PhysicalDevice& physicalDevice, const Device& device, const Mesh& mesh)
	{
		const std::vector<Colour>& colours = mesh.GetColours();
		uint64_t coloursSize = static_cast<uint64_t>(sizeof(uint32_t) * colours.size());

		m_colourBuffer = std::make_unique<Buffer>();
		if (!m_colourBuffer->Initialise(physicalDevice, device, coloursSize,
			vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			vk::SharingMode::eExclusive))
		{
			return nullptr;
		}

		std::unique_ptr<Buffer> colourStagingBuffer = std::make_unique<Buffer>();
		if (!colourStagingBuffer->Initialise(physicalDevice, device, coloursSize,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			vk::SharingMode::eExclusive))
		{
			return nullptr;
		}

		colourStagingBuffer->UpdateContents(device, 0, colours.data(), coloursSize);
		return colourStagingBuffer;
	}

	std::unique_ptr<Buffer> RenderMesh::SetupIndexBuffer(const PhysicalDevice& physicalDevice, const Device& device, const Mesh& mesh)
	{
		const std::vector<uint32_t>& indices = mesh.GetIndices();
		m_indexCount = static_cast<uint32_t>(indices.size());
		uint64_t indicesSize = static_cast<uint64_t>(sizeof(uint32_t) * m_indexCount);

		m_indexBuffer = std::make_unique<Buffer>();
		if (!m_indexBuffer->Initialise(physicalDevice, device, indicesSize,
			vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			vk::SharingMode::eExclusive))
		{
			return nullptr;
		}

		std::unique_ptr<Buffer> indexStagingBuffer = std::make_unique<Buffer>();
		if (!indexStagingBuffer->Initialise(physicalDevice, device, indicesSize,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			vk::SharingMode::eExclusive))
		{
			return nullptr;
		}

		indexStagingBuffer->UpdateContents(device, 0, indices.data(), indicesSize);
		return indexStagingBuffer;
	}

	// hard-coded for now
	struct UniformBufferObject
	{
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 proj;
		glm::mat4 modelViewProj;
	};

	bool RenderMesh::SetupUniformBuffers(const PhysicalDevice& physicalDevice, const Device& device, const Mesh& mesh)
	{
		const vk::Device& deviceImp = device.Get();
		vk::DeviceSize bufferSize = sizeof(UniformBufferObject);

		m_uniformBuffers.resize(m_maxConcurrentFrames);
		m_uniformBuffersMapped.resize(m_maxConcurrentFrames);

		for (uint32_t i = 0; i < m_maxConcurrentFrames; ++i) 
		{
			m_uniformBuffers[i] = std::make_unique<Buffer>();
			Buffer& buffer = *m_uniformBuffers[i];

			if (!buffer.Initialise(physicalDevice, device, bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, vk::SharingMode::eExclusive))
			{
				return false;
			}

			m_uniformBuffersMapped[i] = deviceImp.mapMemory(buffer.GetMemory(), 0, bufferSize);
		}

		return true;
	}

	bool RenderMesh::Initialise(const PhysicalDevice& physicalDevice, const Device& device,
		const CommandPool& resourceCommandPool, const Mesh& mesh)
	{
		const vk::Device& deviceImp = device.Get();

		std::unique_ptr<Buffer> positionStagingBuffer = SetupPositionBuffer(physicalDevice, device, mesh);
		std::unique_ptr<Buffer> colourStagingBuffer = SetupColourBuffer(physicalDevice, device, mesh);
		std::unique_ptr<Buffer> indexStagingBuffer = SetupIndexBuffer(physicalDevice, device, mesh);
		
		if (!positionStagingBuffer.get() || !colourStagingBuffer.get() || !indexStagingBuffer.get())
		{
			return false;
		}

		if (!SetupUniformBuffers(physicalDevice, device, mesh))
		{
			return false;
		}

		vk::DescriptorPoolSize poolSize(vk::DescriptorType::eUniformBuffer, m_maxConcurrentFrames);
		m_descriptorPool = std::make_unique<DescriptorPool>();
		if (!m_descriptorPool->Initialise(device, m_maxConcurrentFrames, { poolSize }))
		{
			return false;
		}

		std::vector<vk::DescriptorSetLayout> layouts;
		for (uint32_t i = 0; i < m_maxConcurrentFrames; ++i)
		{
			layouts.append_range(m_pipelineLayout->GetDescriptorSetLayouts());
		}

		m_descriptorSets = m_descriptorPool->CreateDescriptorSets(device, layouts);

		for (uint32_t i = 0; i < m_maxConcurrentFrames; ++i)
		{
			vk::DescriptorBufferInfo bufferInfo(m_uniformBuffers[i]->Get(), 0, sizeof(UniformBufferObject));
			std::array<vk::DescriptorBufferInfo, 1> bufferInfoArray = { bufferInfo };
			vk::WriteDescriptorSet descriptorWrite(m_descriptorSets[i], 0, 0, vk::DescriptorType::eUniformBuffer, nullptr, bufferInfoArray);
			deviceImp.updateDescriptorSets(1, &descriptorWrite, 0, nullptr);
		}

		// Schedule and execute copies.

		vk::UniqueCommandBuffer positionCopyCommand = positionStagingBuffer->Copy(device, resourceCommandPool, *m_positionBuffer, m_vertexCount * sizeof(glm::vec3));
		vk::UniqueCommandBuffer coloursCopyCommand = colourStagingBuffer->Copy(device, resourceCommandPool, *m_colourBuffer, m_vertexCount * sizeof(uint32_t));
		vk::UniqueCommandBuffer indicesCopyCommand = indexStagingBuffer->Copy(device, resourceCommandPool, *m_indexBuffer, m_indexCount * sizeof(uint32_t));
		std::array<vk::CommandBuffer, 3> copyCommands = { positionCopyCommand.get(), coloursCopyCommand.get(), indicesCopyCommand.get() };

		vk::SubmitInfo submitInfo;
		submitInfo.commandBufferCount = static_cast<uint32_t>(copyCommands.size());
		submitInfo.pCommandBuffers = copyCommands.data();

		const vk::Queue& queue = device.GetGraphicsQueue();
		vk::Result submitResult = queue.submit(1, &submitInfo, nullptr);
		if (submitResult != vk::Result::eSuccess)
		{
			return false;
		}

		queue.waitIdle(); // boo

		return true;
	}

	void RenderMesh::Draw(const vk::CommandBuffer& commandBuffer, const vk::Extent2D& viewSize, uint32_t currentFrameIndex) const
	{
		// Update uniform buffer

		static auto startTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		UniformBufferObject ubo;
		ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.view = glm::lookAt(glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		ubo.proj = glm::perspective(glm::radians(75.0f), viewSize.width / (float)viewSize.height, 0.1f, 10.0f);
		ubo.proj[1][1] *= -1;
		ubo.modelViewProj = ubo.proj * ubo.view * ubo.model;
		memcpy(m_uniformBuffersMapped[currentFrameIndex], &ubo, sizeof(ubo));

		// Draw stuff

		const vk::Pipeline& graphicsPipeline = m_pipelineLayout->GetGraphicsPipeline();
		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);

		std::array<vk::Buffer, 2> vertexBuffers = { m_positionBuffer->Get(), m_colourBuffer->Get() };
		std::array<vk::DeviceSize, 2> offsets = { 0, 0 };
		commandBuffer.bindVertexBuffers(0, vertexBuffers, offsets);

		vk::DeviceSize indexOffset = 0;
		const vk::Buffer& indexBuffer = m_indexBuffer->Get();
		commandBuffer.bindIndexBuffer(indexBuffer, indexOffset, vk::IndexType::eUint32);

		commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout->Get(), 0, m_descriptorSets[currentFrameIndex], nullptr);

		commandBuffer.drawIndexed(m_indexCount, 1, 0, 0, 0);
	}
}