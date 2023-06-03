#pragma once

#include <vulkan/vulkan.hpp>
#include <vma/vk_mem_alloc.h>
#include <Core/Logging/Logger.hpp>

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
		void CopyToImage(const Device& device, uint32_t mipLevel, const vk::CommandBuffer& commandBuffer, const RenderImage& destination) const;
		const VkBuffer& Get() const;

		template <typename T>
		bool GetMappedMemory(T** mappedMemory) const
		{
			if (m_bufferAllocInfo.pMappedData == nullptr)
			{
				Engine::Logging::Logger::Error("Memory is not mapped.");
				return false;
			}

			*mappedMemory = static_cast<T*>(m_bufferAllocInfo.pMappedData);
			return true;
		}

	private:
		VkBuffer m_buffer;
		VmaAllocation m_bufferAlloc;
		VmaAllocationInfo m_bufferAllocInfo;
		VmaAllocator m_allocator; // Bit gnarly, but makes RAII easier.
	};
}