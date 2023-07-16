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
		virtual bool Initialise(const IDevice& device, ImageType imageType, Format format, const glm::uvec3& dimensions,
			uint32_t mipLevels, uint32_t layerCount, ImageTiling tiling, ImageUsageFlags imageUsage, ImageAspectFlags aspectFlags,
			MemoryUsage memoryUsage, AllocationCreateFlags createFlags, SharingMode sharingMode) override;
		virtual bool InitialiseView(const IDevice& device, ImageAspectFlags aspectFlags) override;

		virtual bool UpdateContents(const void* data, size_t size) override;
		virtual void TransitionImageLayout(const IDevice& device, const ICommandBuffer& commandBuffer, ImageLayout newLayout) override;
		virtual void GenerateMipmaps(const IDevice& device, const ICommandBuffer& commandBuffer) override;

		inline const VkImage& Get() const { return m_image; }

	private:
		VkImage m_image;
		VmaAllocation m_imageAlloc;
		VmaAllocationInfo m_imageAllocInfo;
		VmaAllocator m_allocator; // Bit gnarly, but makes RAII easier.
	};
}