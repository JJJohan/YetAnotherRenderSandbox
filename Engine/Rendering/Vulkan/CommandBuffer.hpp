#pragma once

#include <vulkan/vulkan.hpp>

namespace Engine::Rendering::Vulkan
{
	class Device;

	class CommandBuffer
	{
	public:
		CommandBuffer();
		VkCommandBuffer Get() const;
		void Shutdown(const Device& device);
		bool CreateCommandBuffer(const Device& device);

	private:
		VkCommandBuffer m_commandBuffer;
	};
}