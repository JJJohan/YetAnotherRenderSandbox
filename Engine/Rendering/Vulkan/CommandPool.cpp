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

	bool CommandPool::Initialise(const IPhysicalDevice& physicalDevice, const IDevice& device, uint32_t queueFamilyIndex, vk::CommandPoolCreateFlagBits flags)
	{
		const Device& vkDevice = static_cast<const Device&>(device);
		const PhysicalDevice& vkPhysicalDevice = static_cast<const PhysicalDevice&>(physicalDevice);

		QueueFamilyIndices queueFamilyIndices = vkPhysicalDevice.GetQueueFamilyIndices();

		vk::CommandPoolCreateInfo poolInfo(flags, queueFamilyIndex);
		m_commandPool = vkDevice.Get().createCommandPoolUnique(poolInfo);
		if (!m_commandPool.get())
		{
			Logger::Error("Failed to create command pool.");
			return false;
		}

		return true;
	}

	std::vector<vk::UniqueCommandBuffer> CommandPool::CreateCommandBuffers(const IDevice& device, uint32_t bufferCount)
	{
		const Device& vkDevice = static_cast<const Device&>(device);
		vk::CommandBufferAllocateInfo allocInfo(m_commandPool.get(), vk::CommandBufferLevel::ePrimary, bufferCount);
		return vkDevice.Get().allocateCommandBuffersUnique(allocInfo);
	}

	vk::UniqueCommandBuffer CommandPool::BeginResourceCommandBuffer(const IDevice& device) const
	{
		const Device& vkDevice = static_cast<const Device&>(device);
		vk::CommandBufferAllocateInfo allocInfo(m_commandPool.get(), vk::CommandBufferLevel::ePrimary, 1);
		std::vector<vk::UniqueCommandBuffer> commandBuffers = vkDevice.Get().allocateCommandBuffersUnique(allocInfo);
		vk::UniqueCommandBuffer commandBuffer = std::move(commandBuffers.front());

		vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
		commandBuffer->begin(beginInfo);
		return commandBuffer;
	}

}