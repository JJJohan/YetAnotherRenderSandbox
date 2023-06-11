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

	bool CommandPool::Initialise(const PhysicalDevice& physicalDevice, const Device& device, vk::CommandPoolCreateFlagBits flags)
	{
		QueueFamilyIndices queueFamilyIndices = physicalDevice.GetQueueFamilyIndices();

		vk::CommandPoolCreateInfo poolInfo(flags, queueFamilyIndices.GraphicsFamily.value());
		m_commandPool = device.Get().createCommandPoolUnique(poolInfo);
		if (!m_commandPool.get())
		{
			Logger::Error("Failed to create command pool.");
			return false;
		}

		return true;
	}

	std::vector<vk::UniqueCommandBuffer> CommandPool::CreateCommandBuffers(const Device& device, uint32_t bufferCount)
	{
		vk::CommandBufferAllocateInfo allocInfo(m_commandPool.get(), vk::CommandBufferLevel::ePrimary, bufferCount);
		return device.Get().allocateCommandBuffersUnique(allocInfo);
	}

	vk::UniqueCommandBuffer CommandPool::BeginResourceCommandBuffer(const Device& device) const
	{
		vk::CommandBufferAllocateInfo allocInfo(m_commandPool.get(), vk::CommandBufferLevel::ePrimary, 1);
		std::vector<vk::UniqueCommandBuffer> commandBuffers = device.Get().allocateCommandBuffersUnique(allocInfo);
		vk::UniqueCommandBuffer commandBuffer = std::move(commandBuffers.front());

		vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
		commandBuffer->begin(beginInfo);
		return commandBuffer;
	}

}