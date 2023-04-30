#include "RenderImage.hpp"
#include "Device.hpp"
#include "CommandPool.hpp"
#include "Core/Logging/Logger.hpp"

using namespace Engine::Logging;

namespace Engine::Rendering::Vulkan
{
	RenderImage::RenderImage(VmaAllocator allocator)
		: m_image(nullptr)
		, m_imageAlloc(nullptr)
		, m_imageAllocInfo()
		, m_allocator(allocator)
		, m_format(vk::Format::eUndefined)
		, m_layout(vk::ImageLayout::eUndefined)
		, m_dimensions()
	{
	}

	RenderImage::~RenderImage()
	{
		if (m_imageAlloc != nullptr)
			vmaDestroyImage(m_allocator, m_image, m_imageAlloc);
	}

	bool RenderImage::UpdateContents(const void* data, vk::DeviceSize size)
	{
		if (m_imageAllocInfo.pMappedData == nullptr)
		{
			void* mappedData = nullptr;
			VkResult result = vmaMapMemory(m_allocator, m_imageAlloc, &mappedData);
			if (result != VK_SUCCESS)
			{
				Logger::Error("Failed to map memory.");
				return false;
			}

			memcpy(mappedData, data, size);
			vmaUnmapMemory(m_allocator, m_imageAlloc);
			return true;
		}

		memcpy(m_imageAllocInfo.pMappedData, data, size);
		return true;
	}

	bool RenderImage::Initialise(vk::ImageType imageType, vk::Format format, vk::Extent3D dimensions, vk::ImageTiling tiling,
		vk::ImageUsageFlags imageUsage, VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags createFlags, vk::SharingMode sharingMode)
	{
		vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;
		m_format = format;
		m_dimensions = dimensions;

		vk::ImageCreateInfo RenderImageInfo(vk::ImageCreateFlags(), imageType, format, dimensions, 1, 1, samples, tiling, imageUsage, sharingMode);

		VmaAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.usage = memoryUsage;
		allocCreateInfo.flags = createFlags;

		VkImageCreateInfo RenderImageInfoImp = static_cast<VkImageCreateInfo>(RenderImageInfo);
		VkResult createResult = vmaCreateImage(m_allocator, &RenderImageInfoImp, &allocCreateInfo, &m_image, &m_imageAlloc, &m_imageAllocInfo);
		if (createResult != VK_SUCCESS)
		{
			Logger::Error("Failed to create RenderImage.");
			return false;
		}

		return true;
	}

	vk::UniqueCommandBuffer RenderImage::TransitionImageLayout(const Device& device, const CommandPool& commandPool, vk::ImageLayout newLayout)
	{
		vk::UniqueCommandBuffer commandBuffer = commandPool.BeginResourceCommandBuffer(device);

		vk::AccessFlags srcAccessMask;
		vk::AccessFlags dstAccessMask;
		vk::PipelineStageFlags srcStage;
		vk::PipelineStageFlags dstStage;

		if (m_layout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
		{
			srcAccessMask = vk::AccessFlagBits::eNone;
			dstAccessMask = vk::AccessFlagBits::eTransferWrite;
			srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
			dstStage = vk::PipelineStageFlagBits::eTransfer;
		}
		else if (m_layout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
		{
			srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			dstAccessMask = vk::AccessFlagBits::eShaderRead;
			srcStage = vk::PipelineStageFlagBits::eTransfer;
			dstStage = vk::PipelineStageFlagBits::eFragmentShader;
		}
		else 
		{
			srcAccessMask = vk::AccessFlagBits::eNone;
			dstAccessMask = vk::AccessFlagBits::eNone;
			srcStage = vk::PipelineStageFlagBits::eNone;
			dstStage = vk::PipelineStageFlagBits::eNone;
			Logger::Error("Transition layout not supported.");
		}

		if (dstAccessMask != vk::AccessFlagBits::eNone)
		{
			vk::ImageSubresourceRange subResourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

			vk::ImageMemoryBarrier barrier(srcAccessMask, dstAccessMask, m_layout, newLayout,
				VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, m_image, subResourceRange);

			commandBuffer->pipelineBarrier(srcStage, dstStage, vk::DependencyFlagBits::eByRegion, nullptr, nullptr, { barrier });
		}

		commandBuffer->end();

		m_layout = newLayout;
		return commandBuffer;
	}

	const VkImage& RenderImage::Get() const
	{
		return m_image;
	}

	const vk::Extent3D& RenderImage::GetDimensions() const
	{
		return m_dimensions;
	}

	const vk::Format& RenderImage::GetFormat() const
	{
		return m_format;
	}
}