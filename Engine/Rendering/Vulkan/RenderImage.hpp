#pragma once

#include <vulkan/vulkan.hpp>
#include <vma/vk_mem_alloc.h>
#include "../Resources/IRenderImage.hpp"

namespace Engine::Rendering::Vulkan
{
	class PhysicalDevice;

	class RenderImage : public IRenderImage
	{
	public:
		RenderImage(VmaAllocator allocator);
		RenderImage(vk::Image image, vk::Format format, ImageUsageFlags usageFlags);
		RenderImage(const RenderImage&) = delete;

		~RenderImage();
		virtual bool Initialise(std::string_view name, const IDevice& device, ImageType imageType,
			Format format, const glm::uvec3& dimensions, uint32_t mipLevels, uint32_t layerCount,
			ImageTiling tiling, ImageUsageFlags imageUsage, ImageAspectFlags aspectFlags,
			MemoryUsage memoryUsage, AllocationCreateFlags createFlags, SharingMode sharingMode,
			bool preinitialise) override;

		virtual bool InitialiseView(std::string_view name, const IDevice& device, ImageAspectFlags aspectFlags) override;

		virtual bool UpdateContents(const void* data, size_t offset, size_t size) override;

		virtual bool AppendImageLayoutTransition(const ICommandBuffer& commandBuffer,
			ImageLayout newLayout, IMemoryBarriers& memoryBarriers, uint32_t srcQueueFamily,
			uint32_t dstQueueFamily, bool compute) override;

		virtual bool AppendImageLayoutTransitionExt(const ICommandBuffer& commandBuffer,
			MaterialStageFlags newStageFlags, ImageLayout newLayout, MaterialAccessFlags newAccessFlags,
			IMemoryBarriers& memoryBarriers, uint32_t baseMipLevel, uint32_t mipLevelCount,
			uint32_t srcQueueFamily, uint32_t dstQueueFamily, bool compute) override;

		virtual void GenerateMipmaps(const ICommandBuffer& commandBuffer) override;

		virtual bool CreateView(std::string_view name, const IDevice& device, uint32_t baseMipLevel,
			ImageAspectFlags aspectFlags, std::unique_ptr<IImageView>& imageView) const override;

		inline const VkImage& Get() const { return m_image; }

	private:
		bool ProcessQueueFamilyIndices(const ICommandBuffer& commandBuffer, uint32_t& srcQueueFamily,
			uint32_t& dstQueueFamily, vk::AccessFlags2& srcAccessMask, vk::AccessFlags2& dstAccessMask,
			vk::PipelineStageFlags2& srcStage, vk::PipelineStageFlags2& dstStage, ImageLayout& newLayout,
			bool& isQueueRelease);

		VkImage m_image;
		VmaAllocation m_imageAlloc;
		VmaAllocationInfo m_imageAllocInfo;
		VmaAllocator m_allocator; // Bit gnarly, but makes RAII easier.
	};
}