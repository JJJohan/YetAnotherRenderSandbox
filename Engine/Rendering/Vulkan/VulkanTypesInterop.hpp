#pragma once

#include <vulkan/vulkan.hpp>
#include <vma/vk_mem_alloc.h>
#include "../Types.hpp"
#include "ImageView.hpp"
#include "../Resources/AttachmentInfo.hpp"

namespace Engine::Rendering::Vulkan
{
	constexpr inline vk::Filter GetFilter(Filter filter)
	{
		switch (filter)
		{
		case Filter::Nearest:
			return vk::Filter::eNearest;
		case Filter::Linear:
			return vk::Filter::eLinear;
		default:
			throw;
		}
	}

	constexpr inline vk::SamplerMipmapMode GetSamplerMipmapMode(SamplerMipmapMode mode)
	{
		switch (mode)
		{
		case SamplerMipmapMode::Nearest:
			return vk::SamplerMipmapMode::eNearest;
		case SamplerMipmapMode::Linear:
			return vk::SamplerMipmapMode::eLinear;
		default:
			throw;
		}
	}

	constexpr inline vk::ImageAspectFlagBits GetImageAspectFlags(ImageAspectFlags flags)
	{
		switch (flags)
		{
		case ImageAspectFlags::Color:
			return vk::ImageAspectFlagBits::eColor;
		case ImageAspectFlags::Depth:
			return vk::ImageAspectFlagBits::eDepth;
		case ImageAspectFlags::Stencil:
			return vk::ImageAspectFlagBits::eStencil;
		case ImageAspectFlags::None:
			return vk::ImageAspectFlagBits::eNone;
		default:
			throw;
		}
	}

	constexpr inline vk::SamplerAddressMode GetSamplerAddressMode(SamplerAddressMode mode)
	{
		switch (mode)
		{
		case SamplerAddressMode::Repeat:
			return vk::SamplerAddressMode::eRepeat;
		case SamplerAddressMode::MirroredRepeat:
			return vk::SamplerAddressMode::eMirroredRepeat;
		case SamplerAddressMode::ClampToEdge:
			return vk::SamplerAddressMode::eClampToEdge;
		case SamplerAddressMode::ClampToBorder:
			return vk::SamplerAddressMode::eClampToBorder;
		case SamplerAddressMode::MirrorClampToEdge:
			return vk::SamplerAddressMode::eMirrorClampToEdge;
		default:
			throw;
		}
	}

	constexpr inline vk::ImageType GetImageType(ImageType type)
	{
		switch (type)
		{
		case ImageType::e1D:
			return vk::ImageType::e1D;
		case ImageType::e2D:
			return vk::ImageType::e2D;
		case ImageType::e3D:
			return vk::ImageType::e3D;
		default:
			throw;
		}
	}

	constexpr inline vk::ImageTiling GetImageTiling(ImageTiling tiling)
	{
		switch (tiling)
		{
		case ImageTiling::Optimal:
			return vk::ImageTiling::eOptimal;
		case ImageTiling::Linear:
			return vk::ImageTiling::eLinear;
		default:
			throw;
		}
	}

	constexpr inline vk::SharingMode GetSharingMode(SharingMode mode)
	{
		switch (mode)
		{
		case SharingMode::Exclusive:
			return vk::SharingMode::eExclusive;
		case SharingMode::Concurrent:
			return vk::SharingMode::eConcurrent;
		default:
			throw;
		}
	}

	constexpr inline const char* GetProgramTypeName(vk::ShaderStageFlagBits type)
	{
		switch (type)
		{
		case vk::ShaderStageFlagBits::eVertex:
			return "Vertex";
		case vk::ShaderStageFlagBits::eFragment:
			return "Fragment";
		default:
			throw;
		}
	}

	constexpr inline vk::Format GetVulkanFormat(Format format)
	{
		switch (format)
		{
		case Format::R8G8Unorm:
			return vk::Format::eR8G8Unorm;
		case Format::R8G8B8A8Unorm:
			return vk::Format::eR8G8B8A8Unorm;
		case Format::R16G16Sfloat:
			return vk::Format::eR16G16Sfloat;
		case Format::R16G16B16A16Sfloat:
			return vk::Format::eR16G16B16A16Sfloat;
		case Format::R32G32B32A32Sfloat:
			return vk::Format::eR32G32B32A32Sfloat;
		case Format::R16G16B16Sfloat:
			return vk::Format::eR16G16B16Sfloat;
		case Format::R32G32B32Sfloat:
			return vk::Format::eR32G32B32Sfloat;
		case Format::R32G32Sfloat:
			return vk::Format::eR32G32Sfloat;
		case Format::R32Sfloat:
			return vk::Format::eR32Sfloat;
		case Format::D32Sfloat:
			return vk::Format::eD32Sfloat;
		case Format::D32SfloatS8Uint:
			return vk::Format::eD32SfloatS8Uint;
		case Format::D24UnormS8Uint:
			return vk::Format::eD24UnormS8Uint;
		case Format::Bc5UnormBlock:
			return vk::Format::eBc5UnormBlock;
		case Format::Bc7SrgbBlock:
			return vk::Format::eBc7SrgbBlock;
		case Format::Bc7UnormBlock:
			return vk::Format::eBc7UnormBlock;
		case Format::R8G8B8A8Srgb:
			return vk::Format::eR8G8B8A8Srgb;
		case Format::R8G8B8Unorm:
			return vk::Format::eR8G8B8Unorm;
		case Format::B8G8R8A8Unorm:
			return vk::Format::eB8G8R8A8Unorm;
		case Format::A2B10G10R10UnormPack32:
			return vk::Format::eA2B10G10R10UnormPack32;
		case Format::Swapchain:
			return vk::Format::eUndefined; // Queried during building
		default:
			throw;
		}
	}

	constexpr inline Format FromVulkanFormat(vk::Format format)
	{
		switch (format)
		{
		case vk::Format::eR8G8Unorm:
			return Format::R8G8Unorm;
		case vk::Format::eR8G8B8A8Unorm:
			return Format::R8G8B8A8Unorm;
		case vk::Format::eR16G16Sfloat:
			return Format::R16G16Sfloat;
		case vk::Format::eR16G16B16A16Sfloat:
			return Format::R16G16B16A16Sfloat;
		case vk::Format::eR32G32B32A32Sfloat:
			return Format::R32G32B32A32Sfloat;
		case vk::Format::eR16G16B16Sfloat:
			return Format::R16G16B16Sfloat;
		case vk::Format::eR32G32B32Sfloat:
			return Format::R32G32B32Sfloat;
		case vk::Format::eR32G32Sfloat:
			return Format::R32G32Sfloat;
		case vk::Format::eR32Sfloat:
			return Format::R32Sfloat;
		case vk::Format::eD32Sfloat:
			return Format::D32Sfloat;
		case vk::Format::eD32SfloatS8Uint:
			return Format::D32SfloatS8Uint;
		case vk::Format::eD24UnormS8Uint:
			return Format::D24UnormS8Uint;
		case vk::Format::eBc5UnormBlock:
			return Format::Bc5UnormBlock;
		case vk::Format::eBc7SrgbBlock:
			return Format::Bc7SrgbBlock;
		case vk::Format::eBc7UnormBlock:
			return Format::Bc7UnormBlock;
		case vk::Format::eR8G8B8A8Srgb:
			return Format::R8G8B8A8Srgb;
		case vk::Format::eR8G8B8Unorm:
			return Format::R8G8B8Unorm;
		case vk::Format::eB8G8R8A8Unorm:
			return Format::B8G8R8A8Unorm;
		case vk::Format::eA2B10G10R10UnormPack32:
			return Format::A2B10G10R10UnormPack32;
		default:
			throw;
		}
	}

	constexpr inline vk::ImageLayout GetImageLayout(ImageLayout layout)
	{
		switch (layout)
		{
		case ImageLayout::Undefined:
			return vk::ImageLayout::eUndefined;
		case ImageLayout::ColorAttachment:
			return vk::ImageLayout::eColorAttachmentOptimal;
		case ImageLayout::DepthStencilAttachment:
			return vk::ImageLayout::eDepthStencilAttachmentOptimal;
		case ImageLayout::ShaderReadOnly:
			return vk::ImageLayout::eShaderReadOnlyOptimal;
		case ImageLayout::TransferSrc:
			return vk::ImageLayout::eTransferSrcOptimal;
		case ImageLayout::TransferDst:
			return vk::ImageLayout::eTransferDstOptimal;
		case ImageLayout::DepthAttachment:
			return vk::ImageLayout::eDepthAttachmentOptimal;
		case ImageLayout::PresentSrc:
			return vk::ImageLayout::ePresentSrcKHR;
		default:
			throw;
		}
	};

	inline vk::RenderingAttachmentInfo GetAttachmentInfo(const AttachmentInfo& attachmentInfo)
	{
		vk::RenderingAttachmentInfo vkAttachmentInfo{};
		vkAttachmentInfo.imageView = static_cast<const ImageView*>(attachmentInfo.imageView)->Get();
		vkAttachmentInfo.imageLayout = GetImageLayout(attachmentInfo.imageLayout);
		vkAttachmentInfo.loadOp = static_cast<vk::AttachmentLoadOp>(attachmentInfo.loadOp);
		vkAttachmentInfo.storeOp = static_cast<vk::AttachmentStoreOp>(attachmentInfo.storeOp);
		if (vkAttachmentInfo.imageLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
		{
			vkAttachmentInfo.clearValue = vk::ClearValue(vk::ClearDepthStencilValue(attachmentInfo.clearValue.Depth));
		}
		else
		{
			uint32_t colorValue = static_cast<uint32_t>(attachmentInfo.clearValue.Colour);
			vkAttachmentInfo.clearValue = vk::ClearValue(vk::ClearColorValue(colorValue, colorValue, colorValue, colorValue));
		}

		return vkAttachmentInfo;
	}

	constexpr inline vk::Extent2D GetExtent2D(const glm::uvec2& dimensions)
	{
		return vk::Extent2D(dimensions.x, dimensions.y);
	}

	constexpr inline vk::Extent3D GetExtent3D(const glm::uvec3& dimensions)
	{
		return vk::Extent3D(dimensions.x, dimensions.y, dimensions.z);
	}

	constexpr inline VmaMemoryUsage GetVmaMemoryUsage(MemoryUsage memoryUsage)
	{
		switch (memoryUsage)
		{
		case MemoryUsage::Unknown:
			return VMA_MEMORY_USAGE_UNKNOWN;
		case MemoryUsage::GPUOnly:
			return VMA_MEMORY_USAGE_GPU_ONLY;
		case MemoryUsage::CPUOnly:
			return VMA_MEMORY_USAGE_CPU_ONLY;
		case MemoryUsage::CPUToGPU:
			return VMA_MEMORY_USAGE_CPU_TO_GPU;
		case MemoryUsage::GPUToCPU:
			return VMA_MEMORY_USAGE_GPU_TO_CPU;
		case MemoryUsage::CPUCopy:
			return VMA_MEMORY_USAGE_CPU_COPY;
		case MemoryUsage::GPULazilyAllocated:
			return VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED;
		case MemoryUsage::Auto:
			return VMA_MEMORY_USAGE_AUTO;
		case MemoryUsage::AutoPreferDevice:
			return VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
		case MemoryUsage::AutoPreferHost:
			return VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
		default:
			throw;
		}
	}
}