#include "Buffer.hpp"
#include "RenderImage.hpp"
#include "Device.hpp"
#include "CommandPool.hpp"

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

	void Buffer::Copy(const Device& device, const vk::CommandBuffer& commandBuffer, const Buffer& destination, vk::DeviceSize size) const
	{
		vk::BufferCopy copyRegion(0, 0, size);
		commandBuffer.copyBuffer(m_buffer, destination.m_buffer, 1, &copyRegion);
	}

	void Buffer::CopyToImage(const Device& device, uint32_t mipLevel, const vk::CommandBuffer& commandBuffer, const RenderImage& destination) const
	{
		vk::Extent3D extents = destination.GetDimensions();
		for (uint32_t i = 0; i < mipLevel; ++i)
		{
			extents.width /= 2;
			extents.height /= 2;
		}

		vk::ImageSubresourceLayers subresource(vk::ImageAspectFlagBits::eColor, mipLevel, 0, 1);
		vk::BufferImageCopy region(0, 0, 0, subresource, vk::Offset3D(0, 0, 0), extents);
		region.bufferRowLength = extents.width;

		commandBuffer.copyBufferToImage(m_buffer, destination.Get(), vk::ImageLayout::eTransferDstOptimal, { region });
	}

	bool Buffer::Initialise(uint64_t size, vk::BufferUsageFlags bufferUsage,
		VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags createFlags, vk::SharingMode sharingMode)
	{
		m_size = size;
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