#include "Buffer.hpp"
#include "RenderImage.hpp"
#include "VulkanTypesInterop.hpp"
#include "CommandBuffer.hpp"
#include "Device.hpp"
#include "VulkanMemoryBarriers.hpp"

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

	bool Buffer::UpdateContents(const void* data, size_t offset, size_t size)
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

			memcpy((uint8_t*)mappedData + offset, data, size);
			vmaUnmapMemory(m_allocator, m_bufferAlloc);
			return true;
		}

		memcpy((uint8_t*)m_bufferAllocInfo.pMappedData + offset, data, size);
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
		vulkanCommandBuffer.Get().copyBufferToImage(m_buffer, vulkanDestination.Get(), vk::ImageLayout::eTransferDstOptimal, { region });
	}

	bool Buffer::Initialise(std::string_view name, const IDevice& device, uint64_t size, BufferUsageFlags bufferUsage,
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

		const Device& deviceImp = static_cast<const Device&>(device);
		deviceImp.SetResourceName(ResourceType::Buffer, m_buffer, name);

		m_mappedDataPtr = m_bufferAllocInfo.pMappedData;
		return true;
	}

	bool Buffer::ProcessQueueFamilyIndices(const ICommandBuffer& commandBuffer, uint32_t& srcQueueFamily,
		uint32_t& dstQueueFamily, vk::AccessFlags2& srcAccessMask, vk::AccessFlags2& dstAccessMask,
		vk::PipelineStageFlags2& srcStage, vk::PipelineStageFlags2& dstStage)
	{
		if (srcQueueFamily != dstQueueFamily)
		{
			uint32_t commandBufferQueueIndex = commandBuffer.GetQueueFamilyIndex();
			if (commandBufferQueueIndex != srcQueueFamily && commandBufferQueueIndex != dstQueueFamily)
			{
				Logger::Error("Command buffer queue family index matches neither requested source or destination index.");
				return false;
			}

			if (srcQueueFamily == commandBufferQueueIndex) // release
			{
				dstStage = vk::PipelineStageFlagBits2::eTopOfPipe;
				dstAccessMask = vk::AccessFlagBits2::eNone;
			}
			else // acquire
			{
				srcStage = vk::PipelineStageFlagBits2::eBottomOfPipe;
				srcAccessMask = vk::AccessFlagBits2::eNone;
			}
		}
		else
		{
			srcQueueFamily = VK_QUEUE_FAMILY_IGNORED;
			dstQueueFamily = VK_QUEUE_FAMILY_IGNORED;
		}

		return true;
	}

	bool Buffer::AppendBufferMemoryBarrier(const ICommandBuffer& commandBuffer,
		MaterialStageFlags srcStageFlags, MaterialAccessFlags srcAccessFlags,
		MaterialStageFlags dstStageFlags, MaterialAccessFlags dstAccessFlags,
		IMemoryBarriers& memoryBarriers, uint32_t srcQueueFamily,
		uint32_t dstQueueFamily)
	{
		vk::AccessFlags2 srcAccessMask = static_cast<vk::AccessFlagBits2>(srcAccessFlags);
		vk::AccessFlags2 dstAccessMask = static_cast<vk::AccessFlagBits2>(dstAccessFlags);
		vk::PipelineStageFlags2 srcStage = static_cast<vk::PipelineStageFlagBits2>(srcStageFlags);
		vk::PipelineStageFlags2 dstStage = static_cast<vk::PipelineStageFlagBits2>(dstStageFlags);

		if (!ProcessQueueFamilyIndices(commandBuffer, srcQueueFamily, dstQueueFamily,
			srcAccessMask, dstAccessMask, srcStage, dstStage))
			return false;

		VulkanMemoryBarriers& vulkanMemoryBarriers = static_cast<VulkanMemoryBarriers&>(memoryBarriers);
		vulkanMemoryBarriers.AddBufferMemoryBarrier(vk::BufferMemoryBarrier2(srcStage, srcAccessMask, dstStage, dstAccessMask,
			srcQueueFamily, dstQueueFamily, m_buffer, 0, m_size));

		return true;
	}
}