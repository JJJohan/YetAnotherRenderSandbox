#include "RenderImage.hpp"
#include "CommandBuffer.hpp"
#include "Core/Logger.hpp"
#include "VulkanTypesInterop.hpp"
#include "Device.hpp"
#include "VulkanMemoryBarriers.hpp"

namespace Engine::Rendering::Vulkan
{
	RenderImage::RenderImage(VmaAllocator allocator)
		: IRenderImage()
		, m_image(nullptr)
		, m_imageAlloc(nullptr)
		, m_imageAllocInfo()
		, m_allocator(allocator)
	{
	}

	RenderImage::RenderImage(vk::Image image, vk::Format format, ImageUsageFlags usageFlags)
		: IRenderImage()
		, m_image(image)
		, m_imageAlloc(nullptr)
		, m_imageAllocInfo()
		, m_allocator(nullptr)
	{
		m_format = FromVulkanFormat(format);
		m_layerCount = 1;
		m_mipLevels = 1;
		m_usageFlags = usageFlags;
	}

	RenderImage::~RenderImage()
	{
		if (m_imageAlloc != nullptr)
			vmaDestroyImage(m_allocator, m_image, m_imageAlloc);
	}

	bool RenderImage::UpdateContents(const void* data, size_t offset, size_t size)
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

			memcpy((uint8_t*)mappedData + offset, data, size);
			vmaUnmapMemory(m_allocator, m_imageAlloc);
			return true;
		}

		memcpy((uint8_t*)m_imageAllocInfo.pMappedData + offset, data, size);
		return true;
	}

	void RenderImage::GenerateMipmaps(const ICommandBuffer& commandBuffer)
	{
		if (m_mipLevels == 1)
		{
			VulkanMemoryBarriers memoryBarriers{};
			AppendImageLayoutTransition(commandBuffer, ImageLayout::ShaderReadOnly, memoryBarriers, 0, 0, false);
			commandBuffer.MemoryBarrier(memoryBarriers);
			return;
		}

		if (m_layerCount != 1)
		{
			Logger::Error("Generating mip maps for texture arrays is currently not supported.");
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
			blit.srcOffsets = std::array<vk::Offset3D, 2>{ vk::Offset3D(), vk::Offset3D(mipDimensions.x, mipDimensions.y, 1) };
			blit.dstSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, i, 0, 1);
			blit.dstOffsets = std::array<vk::Offset3D, 2>{ vk::Offset3D(), vk::Offset3D(mipDimensions.x > 1 ? mipDimensions.x / 2 : 1, mipDimensions.y > 1 ? mipDimensions.y / 2 : 1, 1) };

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

	bool RenderImage::Initialise(std::string_view name, const IDevice& device, ImageType imageType, Format format, const glm::uvec3& dimensions,
		uint32_t mipLevels, uint32_t layerCount, ImageTiling tiling, ImageUsageFlags imageUsage, ImageAspectFlags aspectFlags,
		MemoryUsage memoryUsage, AllocationCreateFlags createFlags, SharingMode sharingMode, bool preinitialise)
	{
		m_format = format;
		m_dimensions = dimensions;
		m_mipLevels = mipLevels;
		m_layerCount = layerCount;
		m_usageFlags = imageUsage;

		VmaAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.usage = GetVmaMemoryUsage(memoryUsage);
		allocCreateInfo.flags = static_cast<VmaAllocationCreateFlagBits>(createFlags);

		vk::ImageLayout initialLayout = vk::ImageLayout::eUndefined;
		if (preinitialise)
		{
			initialLayout = vk::ImageLayout::ePreinitialized;
			m_layout = ImageLayout::Preinitialised;
		}

		vk::ImageCreateInfo renderImageInfo(vk::ImageCreateFlags(), GetImageType(imageType), GetVulkanFormat(format), GetExtent3D(dimensions),
			mipLevels, layerCount, vk::SampleCountFlagBits::e1, GetImageTiling(tiling), static_cast<vk::ImageUsageFlagBits>(imageUsage), GetSharingMode(sharingMode), {}, initialLayout);


		VkImageCreateInfo renderImageInfoImp = static_cast<VkImageCreateInfo>(renderImageInfo);
		VkResult createResult = vmaCreateImage(m_allocator, &renderImageInfoImp, &allocCreateInfo, &m_image, &m_imageAlloc, &m_imageAllocInfo);
		if (createResult != VK_SUCCESS)
		{
			Logger::Error("Failed to create RenderImage '{}'.", name);
			return false;
		}

		if (preinitialise)
		{
			std::vector<uint8_t> data(dimensions.x * dimensions.y * dimensions.z, 0);
			if (!UpdateContents(data.data(), 0, data.size()))
			{
				return false;
			}
		}

		if (!InitialiseView(name, device, aspectFlags))
		{
			Logger::Error("Failed to create image view for image '{}'.", name);
			return false;
		}

		const Device& deviceImp = static_cast<const Device&>(device);
		deviceImp.SetResourceName(ResourceType::Image, m_image, name);

		return true;
	}

	bool RenderImage::CreateView(std::string_view name, const IDevice& device, uint32_t baseMipLevel, ImageAspectFlags aspectFlags, std::unique_ptr<IImageView>& imageView) const
	{
		if (m_layerCount != 1)
		{
			Logger::Error("Creating image views with more than one layer currently not supported.");
			return false;
		}

		imageView = std::make_unique<ImageView>();
		if (!imageView->Initialise(name, device, *this, baseMipLevel, 1, 1, m_format, aspectFlags))
		{
			Logger::Error("Failed to create image view.");
			return false;
		}

		return true;
	}

	bool RenderImage::InitialiseView(std::string_view name, const IDevice& device, ImageAspectFlags aspectFlags)
	{
		m_imageView = std::make_unique<ImageView>();
		if (!m_imageView->Initialise(name, device, *this, 0, m_mipLevels, m_layerCount, m_format, aspectFlags))
		{
			Logger::Error("Failed to create image view.");
			return false;
		}

		return true;
	}

	static inline bool HasStencilComponent(Format format)
	{
		return format == Format::D32SfloatS8Uint || format == Format::D24UnormS8Uint;
	}

	static inline bool IsDepthFormat(Format format)
	{
		return HasStencilComponent(format) || format == Format::D32Sfloat;
	}

	static inline bool SetFlags(const ImageLayout& srcLayout, const ImageLayout& destLayout, vk::AccessFlags2& accessMask, vk::PipelineStageFlags2& stage, bool input, bool compute)
	{
		switch (srcLayout)
		{
		case ImageLayout::Undefined:
		case ImageLayout::Preinitialised:
			accessMask = vk::AccessFlagBits2::eNone;
			stage = vk::PipelineStageFlagBits2::eTopOfPipe;
			return true;
		case ImageLayout::TransferSrc:
			accessMask = vk::AccessFlagBits2::eTransferRead;
			stage = vk::PipelineStageFlagBits2::eTransfer;
			return true;
		case ImageLayout::TransferDst:
			accessMask = vk::AccessFlagBits2::eTransferWrite;
			stage = vk::PipelineStageFlagBits2::eTransfer;
			return true;
		case ImageLayout::ShaderReadOnly:
			accessMask = vk::AccessFlagBits2::eShaderRead;
			stage = compute ? vk::PipelineStageFlagBits2::eComputeShader : vk::PipelineStageFlagBits2::eFragmentShader;
			return true;
		case ImageLayout::ColorAttachment:
			accessMask = input ? vk::AccessFlagBits2::eColorAttachmentWrite : vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite;
			stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
			return true;
		case ImageLayout::DepthStencilAttachment:
			accessMask = input ? vk::AccessFlagBits2::eDepthStencilAttachmentWrite : vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
			stage = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests;
			return true;
		case ImageLayout::General:
			accessMask = vk::AccessFlagBits2::eShaderWrite;
			stage = vk::PipelineStageFlagBits2::eComputeShader;
			return true;
		case ImageLayout::PresentSrc:
			accessMask = vk::AccessFlagBits2::eNone;
			stage = vk::PipelineStageFlagBits2::eBottomOfPipe;
			return true;
		default:
			return false;
		}
	}

	static inline bool LayoutSupported(ImageUsageFlags flags, ImageLayout layout)
	{
		switch (layout)
		{
		case ImageLayout::ColorAttachment:
			return (flags & ImageUsageFlags::ColorAttachment) == ImageUsageFlags::ColorAttachment;
		case ImageLayout::DepthStencilAttachment:
			return (flags & ImageUsageFlags::DepthStencilAttachment) == ImageUsageFlags::DepthStencilAttachment;
		case ImageLayout::ShaderReadOnly:
			return (flags & ImageUsageFlags::Sampled) == ImageUsageFlags::Sampled;
		case ImageLayout::TransferSrc:
			return (flags & ImageUsageFlags::TransferSrc) == ImageUsageFlags::TransferSrc;
		case ImageLayout::TransferDst:
			return (flags & ImageUsageFlags::TransferDst) == ImageUsageFlags::TransferDst;
		case ImageLayout::General:
			return (flags & ImageUsageFlags::Storage) == ImageUsageFlags::Storage;
		case ImageLayout::PresentSrc:
			return (flags & ImageUsageFlags::ColorAttachment) == ImageUsageFlags::ColorAttachment;
		case ImageLayout::Undefined:
			return true;
		default: // Unexpected layout, return false.
			return false;
		}
	}

	static inline vk::ImageAspectFlags GetAspectFlags(Format format)
	{
		vk::ImageAspectFlags aspectFlags;
		if (IsDepthFormat(format))
		{
			aspectFlags = vk::ImageAspectFlagBits::eDepth;

			if (HasStencilComponent(format))
			{
				aspectFlags |= vk::ImageAspectFlagBits::eStencil;
			}
		}
		else
		{
			aspectFlags = vk::ImageAspectFlagBits::eColor;
		}

		return aspectFlags;
	}

	bool RenderImage::ProcessQueueFamilyIndices(const ICommandBuffer& commandBuffer, uint32_t& srcQueueFamily,
		uint32_t& dstQueueFamily, vk::AccessFlags2& srcAccessMask, vk::AccessFlags2& dstAccessMask,
		vk::PipelineStageFlags2& srcStage, vk::PipelineStageFlags2& dstStage, ImageLayout& newLayout,
		bool& isQueueRelease)
	{
		if (srcQueueFamily != dstQueueFamily)
		{
			uint32_t commandBufferQueueIndex = commandBuffer.GetQueueFamilyIndex();
			if (commandBufferQueueIndex != srcQueueFamily && commandBufferQueueIndex != dstQueueFamily)
			{
				Logger::Error("Command buffer queue family index matches neither requested source or destination index.");
				return false;
			}

			if (commandBufferQueueIndex == srcQueueFamily) // release
			{
				dstStage = vk::PipelineStageFlagBits2::eBottomOfPipe;
				dstAccessMask = vk::AccessFlagBits2::eNone;
				isQueueRelease = true;
			}
			else // acquire
			{
				srcStage = vk::PipelineStageFlagBits2::eTopOfPipe;
				srcAccessMask = vk::AccessFlagBits2::eNone;
			}
		}

		return true;
	}

	bool RenderImage::AppendImageLayoutTransition(const ICommandBuffer& commandBuffer,
		ImageLayout newLayout, IMemoryBarriers& memoryBarriers, uint32_t srcQueueFamily,
		uint32_t dstQueueFamily, bool compute)
	{
		// Skip barrier if it's redundant.
		if (m_layout == newLayout && srcQueueFamily == dstQueueFamily)
			return false;

		if (!LayoutSupported(m_usageFlags, newLayout))
		{
			Logger::Error("Image was not created with usage flags that support the requested layout.");
			return false;
		}

		vk::AccessFlags2 srcAccessMask;
		vk::AccessFlags2 dstAccessMask;
		vk::PipelineStageFlags2 srcStage;
		vk::PipelineStageFlags2 dstStage;

		if (!SetFlags(m_layout, newLayout, srcAccessMask, srcStage, true, compute))
		{
			Logger::Error("Source image layout not handled.");
			return false;
		}

		if (!SetFlags(newLayout, newLayout, dstAccessMask, dstStage, false, compute))
		{
			Logger::Error("Destination image layout not handled.");
			return false;
		}

		bool isQueueRelease = false;
		if (!ProcessQueueFamilyIndices(commandBuffer, srcQueueFamily, dstQueueFamily,
			srcAccessMask, dstAccessMask, srcStage, dstStage, newLayout, isQueueRelease))
			return false;

		if (newLayout == ImageLayout::Undefined)
		{
			Logger::Error("New image layout cannot be undefined.");
			return false;
		}

		ImageLayout oldLayout = m_layout;
		if (srcQueueFamily != commandBuffer.GetQueueFamilyIndex()) // acquire
			oldLayout = ImageLayout::Undefined;

		vk::ImageAspectFlags aspectFlags = GetAspectFlags(m_format);

		vk::ImageSubresourceRange subResourceRange(aspectFlags, 0, m_mipLevels, 0, m_layerCount);

		VulkanMemoryBarriers& vulkanMemoryBarriers = static_cast<VulkanMemoryBarriers&>(memoryBarriers);
		vulkanMemoryBarriers.AddImageMemoryBarrier(vk::ImageMemoryBarrier2(srcStage, srcAccessMask, dstStage, dstAccessMask, GetImageLayout(oldLayout), GetImageLayout(newLayout),
			srcQueueFamily, dstQueueFamily, m_image, subResourceRange));

		if (!isQueueRelease)
			m_layout = newLayout;

		return true;
	}

	bool RenderImage::AppendImageLayoutTransitionExt(const ICommandBuffer& commandBuffer,
		MaterialStageFlags newStageFlags, ImageLayout newLayout, MaterialAccessFlags newAccessFlags,
		IMemoryBarriers& imageMemoryBarriers, uint32_t baseMipLevel, uint32_t mipLevelCount,
		uint32_t srcQueueFamily, uint32_t dstQueueFamily, bool compute)
	{
		if (!LayoutSupported(m_usageFlags, newLayout))
		{
			Logger::Error("Image was not created with usage flags that support the requested layout.");
			return false;
		}

		vk::AccessFlags2 srcAccessMask;
		vk::AccessFlags2 dstAccessMask = static_cast<vk::AccessFlagBits2>(newAccessFlags);
		vk::PipelineStageFlags2 srcStage;
		vk::PipelineStageFlags2 dstStage = static_cast<vk::PipelineStageFlagBits2>(newStageFlags);

		if (!SetFlags(m_layout, newLayout, srcAccessMask, srcStage, true, compute))
		{
			Logger::Error("Source image layout not handled.");
			return false;
		}

		bool isQueueRelease = false;
		if (!ProcessQueueFamilyIndices(commandBuffer, srcQueueFamily, dstQueueFamily,
			srcAccessMask, dstAccessMask, srcStage, dstStage, newLayout, isQueueRelease))
			return false;

		ImageLayout oldLayout = m_layout;
		if (srcQueueFamily != commandBuffer.GetQueueFamilyIndex()) // acquire
			oldLayout = ImageLayout::Undefined;

		vk::ImageAspectFlags aspectFlags = GetAspectFlags(m_format);

		vk::ImageSubresourceRange subResourceRange(aspectFlags, baseMipLevel, mipLevelCount == 0 ? m_mipLevels : mipLevelCount, 0, m_layerCount);

		VulkanMemoryBarriers& vulkanMemoryBarriers = static_cast<VulkanMemoryBarriers&>(imageMemoryBarriers);
		vulkanMemoryBarriers.AddImageMemoryBarrier(vk::ImageMemoryBarrier2(srcStage, srcAccessMask, dstStage, dstAccessMask, GetImageLayout(oldLayout), GetImageLayout(newLayout),
			srcQueueFamily, dstQueueFamily, m_image, subResourceRange));

		if (!isQueueRelease)
			m_layout = newLayout;

		return true;
	}
}