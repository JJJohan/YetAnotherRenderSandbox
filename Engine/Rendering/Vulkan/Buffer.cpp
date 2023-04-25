#include "Buffer.hpp"
#include "PhysicalDevice.hpp"
#include "Device.hpp"
#include "CommandPool.hpp"
#include "Core/Logging/Logger.hpp"
#include <vulkan/vulkan_handles.hpp>

using namespace Engine::Logging;

namespace Engine::Rendering::Vulkan
{
	Buffer::Buffer()
		: m_buffer(nullptr)
		, m_bufferMemory(nullptr)
	{
	}

	bool Buffer::UpdateContents(const Device& device, vk::DeviceSize offset, const void* data, vk::DeviceSize size)
	{
		const vk::Device& deviceImp = device.Get();
		const vk::DeviceMemory& memory = m_bufferMemory.get();

		void* mappedMemory = deviceImp.mapMemory(memory, 0, size);
		memcpy(mappedMemory, data, static_cast<size_t>(size));
		deviceImp.unmapMemory(memory);

		return true;
	}

	vk::UniqueCommandBuffer Buffer::Copy(const Device& device, const CommandPool& commandPool, const Buffer& destination, vk::DeviceSize size)
	{
		vk::CommandBufferAllocateInfo allocInfo(commandPool.Get(), vk::CommandBufferLevel::ePrimary, 1);
		std::vector<vk::UniqueCommandBuffer> commandBuffers = device.Get().allocateCommandBuffersUnique(allocInfo);

		vk::UniqueCommandBuffer commandBuffer = std::move(commandBuffers.front());

		vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
		commandBuffer->begin(beginInfo);

		vk::BufferCopy copyRegion(0, 0, size);
		commandBuffer->copyBuffer(m_buffer.get(), destination.m_buffer.get(), 1, &copyRegion);
		commandBuffer->end();

		return commandBuffer;
	}

	bool Buffer::Initialise(const PhysicalDevice& physicalDevice, const Device& device, uint64_t size, vk::BufferUsageFlags usage,
		vk::MemoryPropertyFlags memoryPropertyFlags, vk::SharingMode sharingMode)
	{
		const vk::Device& deviceImp = device.Get();

		vk::BufferCreateInfo bufferInfo(vk::BufferCreateFlags(), size, usage, sharingMode);
		m_buffer = deviceImp.createBufferUnique(bufferInfo);
		if (!m_buffer.get())
		{
			Logger::Error("Failed to create buffer.");
			return false;
		}

		vk::MemoryRequirements memoryRequirements = deviceImp.getBufferMemoryRequirements(m_buffer.get());

		uint32_t memoryTypeIndex = 0;
		if (!physicalDevice.FindMemoryType(memoryRequirements.memoryTypeBits, memoryPropertyFlags, &memoryTypeIndex))
		{
			Logger::Error("Failed to find memory type for buffer memory.");
			return false;
		}

		vk::MemoryAllocateInfo allocInfo(memoryRequirements.size, memoryTypeIndex);
		m_bufferMemory = deviceImp.allocateMemoryUnique(allocInfo);

		if (!m_bufferMemory.get())
		{
			Logger::Error("Failed to allocate buffer memory.");
			return false;
		}

		deviceImp.bindBufferMemory(m_buffer.get(), m_bufferMemory.get(), 0);

		return true;
	}

	const vk::Buffer& Buffer::Get() const
	{
		return m_buffer.get();
	}
}