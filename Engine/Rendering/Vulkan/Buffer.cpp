#include "Buffer.hpp"
#include "RenderImage.hpp"
#include "VulkanTypesInterop.hpp"
#include "CommandBuffer.hpp"
#include "Device.hpp"

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

	bool Buffer::UpdateContents(const void* data, size_t size)
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

	uint64_t Buffer::GetDeviceAddress(const IDevice& device)
	{
		vk::BufferDeviceAddressInfo info(m_buffer);
		return static_cast<uint64_t>(static_cast<const Device&>(device).Get().getBufferAddress(info));
	}

	void Buffer::Copy(const ICommandBuffer& commandBuffer, const IBuffer& destination, size_t size) const
	{
		const Buffer& vulkanDestination = static_cast<const Buffer&>(destination);
		const CommandBuffer& vulkanCommandBuffer = static_cast<const CommandBuffer&>(commandBuffer);
		vk::BufferCopy copyRegion(0, 0, size);
		vulkanCommandBuffer.Get().copyBuffer(m_buffer, vulkanDestination.Get(), 1, &copyRegion);
	}

	void Buffer::CopyToImage(uint32_t mipLevel, const ICommandBuffer& commandBuffer, const IRenderImage& destination) const
	{
		vk::Extent3D extents = GetExtent3D(destination.GetDimensions());
		for (uint32_t i = 0; i < mipLevel; ++i)
		{
			extents.width /= 2;
			extents.height /= 2;
		}

		vk::ImageSubresourceLayers subresource(vk::ImageAspectFlagBits::eColor, mipLevel, 0, 1);
		vk::BufferImageCopy region(0, 0, 0, subresource, vk::Offset3D(0, 0, 0), extents);
		region.bufferRowLength = extents.width;

		const RenderImage& vulkanDestination = static_cast<const RenderImage&>(destination);
		const CommandBuffer& vulkanCommandBuffer = static_cast<const CommandBuffer&>(commandBuffer);
		vulkanCommandBuffer.Get().copyBufferToImage(m_buffer, vulkanDestination.Get(), vk::ImageLayout::eTransferDstOptimal, {region});
	}

	bool Buffer::Initialise(uint64_t size, BufferUsageFlags bufferUsage,
		MemoryUsage memoryUsage, AllocationCreateFlags createFlags, SharingMode sharingMode)
	{
		m_size = size;
		vk::BufferCreateInfo bufferInfo(vk::BufferCreateFlags(), size, static_cast<vk::BufferUsageFlagBits>(bufferUsage), GetSharingMode(sharingMode));

		VmaAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.usage = GetVmaMemoryUsage(memoryUsage);
		allocCreateInfo.flags = static_cast<VmaAllocatorCreateFlagBits>(createFlags);

		VkBufferCreateInfo bufferInfoImp = static_cast<VkBufferCreateInfo>(bufferInfo);
		VkResult createResult = vmaCreateBuffer(m_allocator, &bufferInfoImp, &allocCreateInfo, &m_buffer, &m_bufferAlloc, &m_bufferAllocInfo);
		if (createResult != VK_SUCCESS)
		{
			Logger::Error("Failed to create buffer.");
			return false;
		}

		m_mappedDataPtr = m_bufferAllocInfo.pMappedData;
		return true;
	}
}