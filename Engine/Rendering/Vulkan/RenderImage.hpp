#pragma once

#include <vulkan/vulkan.hpp>
#include <vma/vk_mem_alloc.h>
#include <glm/glm.hpp>

namespace Engine::Rendering::Vulkan
{
	class Device;
	class CommandPool;

	class RenderImage
	{
	public:
		RenderImage(VmaAllocator allocator);
		~RenderImage();
		bool Initialise(vk::ImageType imageType, vk::Format format, vk::Extent3D dimensions, vk::ImageTiling tiling,
			vk::ImageUsageFlags imageUsage, VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags createFlags, vk::SharingMode sharingMode);
		bool UpdateContents(const void* data, vk::DeviceSize size);
		vk::UniqueCommandBuffer TransitionImageLayout(const Device& device, const CommandPool& commandPool, vk::ImageLayout newLayout);
		const VkImage& Get() const;
		const vk::Extent3D& GetDimensions() const;
		const vk::Format& GetFormat() const;

	private:
		vk::Format m_format;
		VkImage m_image;
		vk::Extent3D m_dimensions;
		vk::ImageLayout m_layout;
		VmaAllocation m_imageAlloc;
		VmaAllocationInfo m_imageAllocInfo;
		VmaAllocator m_allocator; // Bit gnarly, but makes RAII easier.
	};
}