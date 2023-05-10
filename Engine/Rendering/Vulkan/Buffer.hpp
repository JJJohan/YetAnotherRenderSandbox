#pragma once

#include <vulkan/vulkan.hpp>
#include <vma/vk_mem_alloc.h>

namespace Engine::Rendering::Vulkan
{
	class Device;
	class RenderImage;

	class Buffer
	{
	public:
		Buffer(VmaAllocator allocator);
		~Buffer();
		bool Initialise(uint64_t size, vk::BufferUsageFlags bufferUsage,
			VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags createFlags, vk::SharingMode sharingMode);
		bool UpdateContents(const void* data, vk::DeviceSize size);
		void Copy(const Device& device, const vk::CommandBuffer& commandBuffer, const Buffer& destination, vk::DeviceSize size) const;
		void CopyToImage(const Device& device, const vk::CommandBuffer& commandBuffer, const RenderImage& destination) const;
		const VkBuffer& Get() const;
		bool GetMappedMemory(void** mappedMemory) const;

	private:
		VkBuffer m_buffer;
		VmaAllocation m_bufferAlloc;
		VmaAllocationInfo m_bufferAllocInfo;
		VmaAllocator m_allocator; // Bit gnarly, but makes RAII easier.
	};
}