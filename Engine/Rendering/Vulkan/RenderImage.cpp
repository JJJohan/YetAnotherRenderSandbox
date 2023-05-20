#include "RenderImage.hpp"
#include "Device.hpp"
#include "CommandPool.hpp"
#include "Core/Logging/Logger.hpp"
#include "PhysicalDevice.hpp"

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
		, m_mipLevels(1)
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

	bool RenderImage::FormatSupported(const PhysicalDevice& physicalDevice, vk::Format format)
	{
		vk::FormatProperties properties = physicalDevice.Get().getFormatProperties(format);
		return (properties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eTransferDst &&
			properties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImage);
	}

	void RenderImage::GenerateMipmaps(const Device& device, const vk::CommandBuffer& commandBuffer)
	{
		if (m_mipLevels == 1)
		{
			TransitionImageLayout(device, commandBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
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

		vk::Extent3D mipDimensions = m_dimensions;
		for (uint32_t i = 1; i < m_mipLevels; ++i)
		{
			barrier.subresourceRange.baseMipLevel = i - 1;
			barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
			barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
			barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

			commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlagBits::eByRegion,
				nullptr, nullptr, { barrier });

			vk::ImageBlit blit;
			blit.srcSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, i - 1, 0, 1);
			blit.srcOffsets = std::array<vk::Offset3D, 2> { vk::Offset3D(), vk::Offset3D(mipDimensions.width, mipDimensions.height, 1) };
			blit.dstSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, i, 0, 1);
			blit.dstOffsets = std::array<vk::Offset3D, 2> { vk::Offset3D(), vk::Offset3D(mipDimensions.width > 1 ? mipDimensions.width / 2 : 1, mipDimensions.height > 1 ? mipDimensions.height / 2 : 1, 1) };

			commandBuffer.blitImage(m_image, vk::ImageLayout::eTransferSrcOptimal, m_image, vk::ImageLayout::eTransferDstOptimal, { blit }, vk::Filter::eLinear);

			barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
			barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
			barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
			barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

			commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlagBits::eByRegion,
				nullptr, nullptr, { barrier });

			if (mipDimensions.width > 1) mipDimensions.width /= 2;
			if (mipDimensions.height > 1) mipDimensions.height /= 2;
		}

		barrier.subresourceRange.baseMipLevel = m_mipLevels - 1;
		barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
		barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

		commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlagBits::eByRegion,
			nullptr, nullptr, { barrier });
	}

	bool RenderImage::Initialise(vk::ImageType imageType, vk::Format format, vk::Extent3D dimensions, vk::SampleCountFlagBits sampleCount, uint32_t mipLevels, vk::ImageTiling tiling,
		vk::ImageUsageFlags imageUsage, VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags createFlags, vk::SharingMode sharingMode)
	{
		m_format = format;
		m_dimensions = dimensions;

		m_mipLevels = mipLevels;

		vk::ImageCreateInfo RenderImageInfo(vk::ImageCreateFlags(), imageType, format, dimensions, m_mipLevels, 1, sampleCount, tiling, imageUsage, sharingMode);

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

	bool HasStencilComponent(vk::Format format)
	{
		return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
	}

	void RenderImage::TransitionImageLayout(const Device& device, const vk::CommandBuffer& commandBuffer, vk::ImageLayout newLayout)
	{
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
		else if (m_layout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
		{
			srcAccessMask = vk::AccessFlagBits::eNone;;
			dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
			srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
			dstStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
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
			vk::ImageAspectFlags aspectFlags;
			if (newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
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

			vk::ImageMemoryBarrier barrier(srcAccessMask, dstAccessMask, m_layout, newLayout,
				VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, m_image, subResourceRange);

			commandBuffer.pipelineBarrier(srcStage, dstStage, vk::DependencyFlagBits::eByRegion, nullptr, nullptr, { barrier });
		}

		m_layout = newLayout;
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

	uint32_t RenderImage::GetMiplevels() const
	{
		return m_mipLevels;
	}
}