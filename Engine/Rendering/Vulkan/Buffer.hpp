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
		virtual bool Initialise(std::string_view name, const IDevice& device, uint64_t size, BufferUsageFlags bufferUsage,
			MemoryUsage memoryUsage, AllocationCreateFlags createFlags, SharingMode sharingMode) override;
		virtual bool UpdateContents(const void* data, size_t offset, size_t size) override;
		virtual uint64_t GetDeviceAddress(const IDevice& device) override;
		virtual void Copy(const ICommandBuffer& commandBuffer, const IBuffer& destination, size_t size) const override;
		virtual void CopyToImage(uint32_t mipLevel, const ICommandBuffer& commandBuffer, const IRenderImage& destination) const override;

		bool ProcessQueueFamilyIndices(const ICommandBuffer& commandBuffer, uint32_t& srcQueueFamily,
			uint32_t& dstQueueFamily, vk::AccessFlags2& srcAccessMask, vk::AccessFlags2& dstAccessMask,
			vk::PipelineStageFlags2& srcStage, vk::PipelineStageFlags2& dstStage);

		virtual bool AppendBufferMemoryBarrier(const ICommandBuffer& commandBuffer,
			MaterialStageFlags srcStageFlags, MaterialAccessFlags srcAccessFlags,
			MaterialStageFlags dstStageFlags, MaterialAccessFlags dstAccessFlags,
			IMemoryBarriers& memoryBarriers, uint32_t srcQueueFamily = 0,
			uint32_t dstQueueFamily = 0) override;

		inline const VkBuffer& Get() const { return m_buffer; }

	private:
		VkBuffer m_buffer;
		VmaAllocation m_bufferAlloc;
		VmaAllocationInfo m_bufferAllocInfo;
		VmaAllocator m_allocator; // Bit gnarly, but makes RAII easier.
	};
}