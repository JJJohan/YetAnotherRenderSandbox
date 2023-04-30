#include "Buffer.hpp"
#include "RenderImage.hpp"
#include "Device.hpp"
#include "CommandPool.hpp"
#include "Core/Logging/Logger.hpp"

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
			return true;
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
		vk::UniqueCommandBuffer commandBuffer = commandPool.BeginResourceCommandBuffer(device);

		vk::BufferCopy copyRegion(0, 0, size);
		commandBuffer->copyBuffer(m_buffer, destination.m_buffer, 1, &copyRegion);
		commandBuffer->end();

		return commandBuffer;
	}

	vk::UniqueCommandBuffer Buffer::CopyToImage(const Device& device, const CommandPool& commandPool, const RenderImage& destination) const
	{
		vk::UniqueCommandBuffer commandBuffer = commandPool.BeginResourceCommandBuffer(device);

		vk::ImageSubresourceLayers subresource(vk::ImageAspectFlagBits::eColor, 0, 0, 1);
		vk::BufferImageCopy region(0, 0, 0, subresource, vk::Offset3D(0, 0, 0), destination.GetDimensions());

		commandBuffer->copyBufferToImage(m_buffer, destination.Get(), vk::ImageLayout::eTransferDstOptimal, { region });

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