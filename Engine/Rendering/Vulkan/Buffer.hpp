#pragma once

#include <vulkan/vulkan.hpp>

namespace Engine::Rendering::Vulkan
{
	class Device;
	class PhysicalDevice;
	class CommandPool;

	class Buffer
	{
	public:
		Buffer();
		bool Initialise(const PhysicalDevice& physicalDevice, const Device& device, uint64_t size, vk::BufferUsageFlags usage,
			vk::MemoryPropertyFlags memoryPropertyFlags, vk::SharingMode sharingMode);
		bool UpdateContents(const Device& device, vk::DeviceSize offset, const void* data, vk::DeviceSize size);
		vk::UniqueCommandBuffer Copy(const Device& device, const CommandPool& commandPool, const Buffer& destination, vk::DeviceSize size) const;
		const vk::Buffer& Get() const;
		const vk::DeviceMemory& GetMemory() const;

	private:
		vk::UniqueBuffer m_buffer;
		vk::UniqueDeviceMemory m_bufferMemory;
	};
}