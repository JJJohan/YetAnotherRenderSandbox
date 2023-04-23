#include "CommandPool.hpp"
#include "Core/Logging/Logger.hpp"
#include "Device.hpp"
#include "PhysicalDevice.hpp"
#include "QueueFamilyIndices.hpp"

using namespace Engine::Logging;

namespace Engine::Rendering::Vulkan
{
	CommandPool::CommandPool()
		: m_commandPool(nullptr)
	{

	}

	void CommandPool::Shutdown(const Device& device)
	{
		if (m_commandPool)
		{
			vkDestroyCommandPool(device.Get(), m_commandPool, nullptr);
			m_commandPool = nullptr;
		}
	}

	bool CommandPool::CreateCommandPool(const PhysicalDevice& physicalDevice, const Device& device)
	{
		QueueFamilyIndices queueFamilyIndices = physicalDevice.GetQueueFamilyIndices();

		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = queueFamilyIndices.GraphicsFamily.value();


		if (vkCreateCommandPool(device.Get(), &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS)
		{
			Logger::Error("Failed to create command pool.");
			return false;
		}

		return true;
	}

	bool CommandPool::CreateCommandBuffers(const Device& device, VkCommandBuffer* commandBuffers, uint32_t bufferCount)
	{
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = m_commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = bufferCount;

		if (vkAllocateCommandBuffers(device.Get(), &allocInfo, commandBuffers) != VK_SUCCESS)
		{
			Logger::Error("Failed to allocate command buffers.");
			return false;
		}

		return true;
	}
}