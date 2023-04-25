#include "RenderMesh.hpp"
#include "PhysicalDevice.hpp"
#include "Device.hpp"
#include "CommandBuffer.hpp"
#include "CommandPool.hpp"
#include "PipelineLayout.hpp"
#include "../Mesh.hpp"
#include "Core/Logging/Logger.hpp"
#include <glm/glm.hpp>

using namespace Engine::Logging;

namespace Engine::Rendering::Vulkan
{
	RenderMesh::RenderMesh(const PipelineLayout* pipelineLayout)
		: m_positionBuffer(nullptr)
		, m_colourBuffer(nullptr)
		, m_indexBuffer(nullptr)
		, m_pipelineLayout(pipelineLayout)
		, m_vertexCount(0)
		, m_indexCount(0)
	{
	}

	bool RenderMesh::Initialise(const PhysicalDevice& physicalDevice, const Device& device,
		const CommandPool& resourceCommandPool, const Mesh& mesh)
	{
		const vk::Device& deviceImp = device.Get();

		const std::vector<glm::vec3>& positions = mesh.GetPositions();
		m_vertexCount = static_cast<uint32_t>(positions.size());
		uint64_t positionsSize = static_cast<uint64_t>(sizeof(glm::vec3) * m_vertexCount);

		m_positionBuffer = std::make_unique<Buffer>();
		if (!m_positionBuffer->Initialise(physicalDevice, device, positionsSize,
			vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			vk::SharingMode::eExclusive))
		{
			return false;
		}

		std::unique_ptr<Buffer> positionStagingBuffer = std::make_unique<Buffer>();
		if (!positionStagingBuffer->Initialise(physicalDevice, device, positionsSize,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			vk::SharingMode::eExclusive))
		{
			return false;
		}

		positionStagingBuffer->UpdateContents(device, 0, positions.data(), positionsSize);

		const std::vector<Colour>& colours = mesh.GetColours();
		uint64_t coloursSize = static_cast<uint64_t>(sizeof(uint32_t) * colours.size());

		m_colourBuffer = std::make_unique<Buffer>();
		if (!m_colourBuffer->Initialise(physicalDevice, device, coloursSize,
			vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			vk::SharingMode::eExclusive))
		{
			return false;
		}

		std::unique_ptr<Buffer> colourStagingBuffer = std::make_unique<Buffer>();
		if (!colourStagingBuffer->Initialise(physicalDevice, device, coloursSize,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			vk::SharingMode::eExclusive))
		{
			return false;
		}

		colourStagingBuffer->UpdateContents(device, 0, colours.data(), coloursSize);

		const std::vector<uint32_t>& indices = mesh.GetIndices();
		m_indexCount = static_cast<uint32_t>(indices.size());
		uint64_t indicesSize = static_cast<uint64_t>(sizeof(uint32_t) * m_indexCount);

		m_indexBuffer = std::make_unique<Buffer>();
		if (!m_indexBuffer->Initialise(physicalDevice, device, indicesSize,
			vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			vk::SharingMode::eExclusive))
		{
			return false;
		}

		std::unique_ptr<Buffer> indexStagingBuffer = std::make_unique<Buffer>();
		if (!indexStagingBuffer->Initialise(physicalDevice, device, indicesSize,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			vk::SharingMode::eExclusive))
		{
			return false;
		}

		indexStagingBuffer->UpdateContents(device, 0, indices.data(), indicesSize);

		// Schedule and execute copies.

		vk::UniqueCommandBuffer positionCopyCommand = positionStagingBuffer->Copy(device, resourceCommandPool, *m_positionBuffer, positionsSize);
		vk::UniqueCommandBuffer coloursCopyCommand = colourStagingBuffer->Copy(device, resourceCommandPool, *m_colourBuffer, coloursSize);
		vk::UniqueCommandBuffer indicesCopyCommand = indexStagingBuffer->Copy(device, resourceCommandPool, *m_indexBuffer, indicesSize);
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

	void RenderMesh::Draw(const vk::CommandBuffer& commandBuffer) const
	{
		const vk::Pipeline& graphicsPipeline = m_pipelineLayout->GetGraphicsPipeline();
		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);

		std::array<vk::Buffer, 2> vertexBuffers = { m_positionBuffer->Get(), m_colourBuffer->Get() };
		std::array<vk::DeviceSize, 2> offsets = { 0, 0 };
		commandBuffer.bindVertexBuffers(0, vertexBuffers, offsets);

		vk::DeviceSize indexOffset = 0;
		const vk::Buffer& indexBuffer = m_indexBuffer->Get();
		commandBuffer.bindIndexBuffer(indexBuffer, indexOffset, vk::IndexType::eUint32);

		commandBuffer.drawIndexed(m_indexCount, 1, 0, 0, 0);
	}
}