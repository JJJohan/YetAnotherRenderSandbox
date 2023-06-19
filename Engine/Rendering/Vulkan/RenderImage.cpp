#include "RenderImage.hpp"
#include "CommandBuffer.hpp"
#include "Core/Logging/Logger.hpp"
#include "VulkanTypesInterop.hpp"

using namespace Engine::Logging;

namespace Engine::Rendering::Vulkan
{
	RenderImage::RenderImage(VmaAllocator allocator)
		: m_image(nullptr)
		, m_imageAlloc(nullptr)
		, m_imageAllocInfo()
		, m_allocator(allocator)
	{
	}

	RenderImage::RenderImage(vk::Image image, vk::Format format)
		: m_image(image)
		, m_imageAlloc(nullptr)
		, m_imageAllocInfo()
		, m_allocator(nullptr)
	{
	}

	RenderImage::~RenderImage()
	{
		if (m_imageAlloc != nullptr)
			vmaDestroyImage(m_allocator, m_image, m_imageAlloc);
	}

	bool RenderImage::UpdateContents(const void* data, size_t size)
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

	void RenderImage::GenerateMipmaps(const IDevice& device, const ICommandBuffer& commandBuffer)
	{
		if (m_mipLevels == 1)
		{
			TransitionImageLayout(device, commandBuffer, ImageLayout::ShaderReadOnly);
			return;
		}

		vk::ImageMemoryBarrier barrier;
		barrier.image = m_image;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.levelCount = 1;

		const vk::CommandBuffer& vulkanCommandBuffer = static_cast<const CommandBuffer&>(commandBuffer).Get();

		glm::uvec3 mipDimensions = m_dimensions;
		for (uint32_t i = 1; i < m_mipLevels; ++i)
		{
			barrier.subresourceRange.baseMipLevel = i - 1;
			barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
			barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
			barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

			vulkanCommandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlagBits::eByRegion,
				nullptr, nullptr, { barrier });

			vk::ImageBlit blit;
			blit.srcSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, i - 1, 0, 1);
			blit.srcOffsets = std::array<vk::Offset3D, 2> { vk::Offset3D(), vk::Offset3D(mipDimensions.x, mipDimensions.y, 1) };
			blit.dstSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, i, 0, 1);
			blit.dstOffsets = std::array<vk::Offset3D, 2> { vk::Offset3D(), vk::Offset3D(mipDimensions.x > 1 ? mipDimensions.x / 2 : 1, mipDimensions.y > 1 ? mipDimensions.y / 2 : 1, 1) };

			vulkanCommandBuffer.blitImage(m_image, vk::ImageLayout::eTransferSrcOptimal, m_image, vk::ImageLayout::eTransferDstOptimal, { blit }, vk::Filter::eLinear);

			barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
			barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
			barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
			barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

			vulkanCommandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlagBits::eByRegion,
				nullptr, nullptr, { barrier });

			if (mipDimensions.x > 1) mipDimensions.x /= 2;
			if (mipDimensions.y > 1) mipDimensions.y /= 2;
		}

		barrier.subresourceRange.baseMipLevel = m_mipLevels - 1;
		barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
		barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

		vulkanCommandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlagBits::eByRegion,
			nullptr, nullptr, { barrier });
	}

	bool RenderImage::Initialise(ImageType imageType, Format format, const glm::uvec3& dimensions, uint32_t mipLevels, ImageTiling tiling,
		ImageUsageFlags imageUsage, MemoryUsage memoryUsage, AllocationCreateFlags createFlags, SharingMode sharingMode)
	{
		m_format = format;
		m_dimensions = dimensions;

		m_mipLevels = mipLevels;

		vk::ImageCreateInfo RenderImageInfo(vk::ImageCreateFlags(), GetImageType(imageType), GetVulkanFormat(format), GetExtent3D(dimensions),
			m_mipLevels, 1, vk::SampleCountFlagBits::e1, GetImageTiling(tiling), static_cast<vk::ImageUsageFlagBits>(imageUsage), GetSharingMode(sharingMode));

		VmaAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.usage = GetVmaMemoryUsage(memoryUsage);
		allocCreateInfo.flags = static_cast<VmaAllocationCreateFlagBits>(createFlags);

		VkImageCreateInfo RenderImageInfoImp = static_cast<VkImageCreateInfo>(RenderImageInfo);
		VkResult createResult = vmaCreateImage(m_allocator, &RenderImageInfoImp, &allocCreateInfo, &m_image, &m_imageAlloc, &m_imageAllocInfo);
		if (createResult != VK_SUCCESS)
		{
			Logger::Error("Failed to create RenderImage.");
			return false;
		}

		return true;
	}

	bool HasStencilComponent(Format format)
	{
		return format == Format::D32SfloatS8Uint || format == Format::D24UnormS8Uint;
	}

	inline bool SetFlags(const ImageLayout& imageLayout, vk::AccessFlags& accessMask, vk::PipelineStageFlags& stage)
	{
		switch (imageLayout)
		{
		case ImageLayout::Undefined:
			accessMask = vk::AccessFlagBits::eNone;
			stage = vk::PipelineStageFlagBits::eTopOfPipe;
			return true;
		case ImageLayout::TransferSrc:
			accessMask = vk::AccessFlagBits::eTransferRead;
			stage = vk::PipelineStageFlagBits::eTransfer;
			return true;
		case ImageLayout::TransferDst:
			accessMask = vk::AccessFlagBits::eTransferWrite;
			stage = vk::PipelineStageFlagBits::eTransfer;
			return true;
		case ImageLayout::ShaderReadOnly:
			accessMask = vk::AccessFlagBits::eShaderRead;
			stage = vk::PipelineStageFlagBits::eFragmentShader;
			return true;
		case ImageLayout::ColorAttachment:
			accessMask = vk::AccessFlagBits::eColorAttachmentWrite;
			stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
			return true;
		case ImageLayout::DepthStencilAttachment:
			accessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
			stage = vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests;
			return true;
		case ImageLayout::PresentSrc:
			accessMask = vk::AccessFlagBits::eNone;
			stage = vk::PipelineStageFlagBits::eBottomOfPipe;
			return true;
		default:
			return false;
		}
	}

	void RenderImage::TransitionImageLayout(const IDevice& device, const ICommandBuffer& commandBuffer, ImageLayout newLayout)
	{
		if (m_layout == newLayout)
			return;

		vk::AccessFlags srcAccessMask;
		vk::AccessFlags dstAccessMask;
		vk::PipelineStageFlags srcStage;
		vk::PipelineStageFlags dstStage;

		if (!SetFlags(m_layout, srcAccessMask, srcStage))
		{
			Logger::Error("Source image layout not handled.");
			return;
		}

		if (!SetFlags(newLayout, dstAccessMask, dstStage))
		{
			Logger::Error("Destination image layout not handled.");
			return;
		}

		vk::ImageAspectFlags aspectFlags;
		if (m_layout == ImageLayout::DepthStencilAttachment || newLayout == ImageLayout::DepthStencilAttachment)
		{
			aspectFlags = vk::ImageAspectFlagBits::eDepth;

			if (HasStencilComponent(m_format))
			{
				aspectFlags |= vk::ImageAspectFlagBits::eStencil;
			}
		}
		else
		{
			aspectFlags = vk::ImageAspectFlagBits::eColor;
		}

		vk::ImageSubresourceRange subResourceRange(aspectFlags, 0, m_mipLevels, 0, 1);

		vk::ImageMemoryBarrier barrier(srcAccessMask, dstAccessMask, GetImageLayout(m_layout), GetImageLayout(newLayout),
			VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, m_image, subResourceRange);

		const CommandBuffer& vulkanCommandBuffer = static_cast<const CommandBuffer&>(commandBuffer);
		vulkanCommandBuffer.Get().pipelineBarrier(srcStage, dstStage,
			vk::DependencyFlagBits::eByRegion, nullptr, nullptr, {barrier});

		m_layout = newLayout;
	}
}