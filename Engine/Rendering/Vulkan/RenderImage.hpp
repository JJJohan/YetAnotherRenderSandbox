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
		RenderImage(vk::Image image, vk::Format format);
		~RenderImage();
		virtual bool Initialise(ImageType imageType, Format format, const glm::uvec3& dimensions, uint32_t mipLevels, ImageTiling tiling,
			ImageUsageFlags imageUsage, MemoryUsage memoryUsage, AllocationCreateFlags createFlags, SharingMode sharingMode) override;
		virtual bool UpdateContents(const void* data, size_t size) override;
		virtual void TransitionImageLayout(const IDevice& device, const ICommandBuffer& commandBuffer, ImageLayout newLayout) override;
		virtual void GenerateMipmaps(const IDevice& device, const ICommandBuffer& commandBuffer) override;

		inline const VkImage& Get() const { return m_image; }

		static bool FormatSupported(const PhysicalDevice& physicalDevice, vk::Format format);

	private:
		VkImage m_image;
		VmaAllocation m_imageAlloc;
		VmaAllocationInfo m_imageAllocInfo;
		VmaAllocator m_allocator; // Bit gnarly, but makes RAII easier.
	};
}