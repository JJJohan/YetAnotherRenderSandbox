#include "Buffer.hpp"
#include "Device.hpp"
#include "CommandPool.hpp"
#include "Core/Logging/Logger.hpp"
#include <vma/vk_mem_alloc.h>

using namespace Engine::Logging;

namespace Engine::Rendering::Vulkan
{
	Buffer::Buffer(VmaAllocator allocator)
		: m_buffer(nullptr)
		, m_bufferAlloc(nullptr)
		, m_bufferAllocInfo()
		, m_allocator(allocator)
	{
	}

	Buffer::~Buffer()
	{
		if (m_bufferAlloc != nullptr)
			vmaDestroyBuffer(m_allocator, m_buffer, m_bufferAlloc);
	}

	bool Buffer::UpdateContents(const void* data, vk::DeviceSize size)
	{
		if (m_bufferAllocInfo.pMappedData == nullptr)
		{
			void* mappedData = nullptr;
			VkResult result = vmaMapMemory(m_allocator, m_bufferAlloc, &mappedData);
			if (result != VK_SUCCESS)
			{
				Logger::Error("Failed to map memory.");
				return false;
			}

			memcpy(mappedData, data, size);
			vmaUnmapMemory(m_allocator, m_bufferAlloc);
		}

		memcpy(m_bufferAllocInfo.pMappedData, data, size);
		return true;
	}

	bool Buffer::GetMappedMemory(void** mappedMemory) const
	{
		if (m_bufferAllocInfo.pMappedData == nullptr)
		{
			Logger::Error("Memory is not mapped.");
			return false;
		}

		*mappedMemory = m_bufferAllocInfo.pMappedData;
		return true;
	}

	vk::UniqueCommandBuffer Buffer::Copy(const Device& device, const CommandPool& commandPool, const Buffer& destination, vk::DeviceSize size) const
	{
		vk::CommandBufferAllocateInfo allocInfo(commandPool.Get(), vk::CommandBufferLevel::ePrimary, 1);
		std::vector<vk::UniqueCommandBuffer> commandBuffers = device.Get().allocateCommandBuffersUnique(allocInfo);

		vk::UniqueCommandBuffer commandBuffer = std::move(commandBuffers.front());

		vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
		commandBuffer->begin(beginInfo);


		vk::BufferCopy copyRegion(0, 0, size);
		commandBuffer->copyBuffer(m_buffer, destination.m_buffer, 1, &copyRegion);
		commandBuffer->end();

		return commandBuffer;
	}

	bool Buffer::Initialise(uint64_t size, vk::BufferUsageFlags bufferUsage,
		VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags createFlags, vk::SharingMode sharingMode)
	{
		vk::BufferCreateInfo bufferInfo(vk::BufferCreateFlags(), size, bufferUsage, sharingMode);

		VmaAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.usage = memoryUsage;
		allocCreateInfo.flags = createFlags;

		VkBuffer stagingVertexBuffer = nullptr;
		VmaAllocation stagingVertexBufferAlloc = VK_NULL_HANDLE;
		VkBufferCreateInfo bufferInfoImp = static_cast<VkBufferCreateInfo>(bufferInfo);
		VkResult createResult = vmaCreateBuffer(m_allocator, &bufferInfoImp, &allocCreateInfo, &m_buffer, &m_bufferAlloc, &m_bufferAllocInfo);
		if (createResult != VK_SUCCESS)
		{
			Logger::Error("Failed to create buffer.");
			return false;
		}

		return true;
	}

	const VkBuffer& Buffer::Get() const
	{
		return m_buffer;
	}
}