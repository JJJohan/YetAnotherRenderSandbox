#pragma once

#include <vulkan/vulkan.hpp>
#include <vma/vk_mem_alloc.h>
#include <glm/glm.hpp>

namespace Engine::Rendering::Vulkan
{
	class Device;
	class PhysicalDevice;

	class RenderImage
	{
	public:
		RenderImage(VmaAllocator allocator);
		RenderImage(vk::Image image, vk::Format format);
		~RenderImage();
		bool Initialise(vk::ImageType imageType, vk::Format format, vk::Extent3D dimensions, uint32_t mipLevels, vk::ImageTiling tiling,
			vk::ImageUsageFlags imageUsage, VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags createFlags, vk::SharingMode sharingMode);
		bool UpdateContents(const void* data, vk::DeviceSize size);
		void TransitionImageLayout(const Device& device, const vk::CommandBuffer& commandBuffer, vk::ImageLayout newLayout);
		void GenerateMipmaps(const Device& device, const vk::CommandBuffer& commandBuffer);

		inline const VkImage& Get() const { return m_image; }
		inline const vk::Extent3D& GetDimensions() const { return m_dimensions; }
		inline const vk::Format& GetFormat() const { return m_format; }
		inline uint32_t GetMiplevels() const { return m_mipLevels; }
		inline const vk::ImageLayout& GetLayout() const { return m_layout; }

		static bool FormatSupported(const PhysicalDevice& physicalDevice, vk::Format format);

	private:
		vk::Format m_format;
		VkImage m_image;
		uint32_t m_mipLevels;
		vk::Extent3D m_dimensions;
		vk::ImageLayout m_layout;
		VmaAllocation m_imageAlloc;
		VmaAllocationInfo m_imageAllocInfo;
		VmaAllocator m_allocator; // Bit gnarly, but makes RAII easier.
	};
}