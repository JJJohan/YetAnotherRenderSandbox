#pragma once

#include <vulkan/vulkan.hpp>
#include <vma/vk_mem_alloc.h>
#include "../Resources/IBuffer.hpp"

namespace Engine::Rendering::Vulkan
{
	class Buffer : public IBuffer
	{
	public:
		Buffer(VmaAllocator allocator);
		~Buffer();
		virtual bool Initialise(uint64_t size, BufferUsageFlags bufferUsage,
			MemoryUsage memoryUsage, AllocationCreateFlags createFlags, SharingMode sharingMode) override;
		virtual bool UpdateContents(const void* data, size_t size) override;
		virtual void Copy(const ICommandBuffer& commandBuffer, const IBuffer& destination, size_t size) const override;
		virtual void CopyToImage(uint32_t mipLevel, const ICommandBuffer& commandBuffer, const IRenderImage& destination) const override;

		inline const VkBuffer& Get() const { return m_buffer; }

	private:
		VkBuffer m_buffer;
		VmaAllocation m_bufferAlloc;
		VmaAllocationInfo m_bufferAllocInfo;
		VmaAllocator m_allocator; // Bit gnarly, but makes RAII easier.
	};
}