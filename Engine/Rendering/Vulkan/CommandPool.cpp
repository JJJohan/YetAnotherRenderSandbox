#include "CommandPool.hpp"
#include "Core/Logging/Logger.hpp"
#include "Device.hpp"
#include "PhysicalDevice.hpp"
#include "CommandBuffer.hpp"

using namespace Engine::Logging;

namespace Engine::Rendering::Vulkan
{
	CommandPool::CommandPool()
		: m_commandPool(nullptr)
	{
	}

	CommandPool::~CommandPool()
	{
		m_commandPool.reset();
	}

	bool CommandPool::Initialise(const char* name, const IPhysicalDevice& physicalDevice, const IDevice& device, uint32_t queueFamilyIndex, CommandPoolFlags flags)
	{
		const Device& vkDevice = static_cast<const Device&>(device);
		const PhysicalDevice& vkPhysicalDevice = static_cast<const PhysicalDevice&>(physicalDevice);

		vk::CommandPoolCreateInfo poolInfo(static_cast<vk::CommandPoolCreateFlagBits>(flags), queueFamilyIndex);
		m_commandPool = vkDevice.Get().createCommandPoolUnique(poolInfo);
		if (!m_commandPool.get())
		{
			Logger::Error("Failed to create command pool.");
			return false;
		}

		vkDevice.SetResourceName(ResourceType::CommandPool, m_commandPool.get(), name);

		return true;
	}

	std::vector<std::unique_ptr<ICommandBuffer>> CommandPool::CreateCommandBuffers(const char* name, const IDevice& device, uint32_t count) const
	{
		const Device& vkDevice = static_cast<const Device&>(device);
		vk::CommandBufferAllocateInfo allocInfo(m_commandPool.get(), vk::CommandBufferLevel::ePrimary, count);
		std::vector<vk::UniqueCommandBuffer> commandBuffers = vkDevice.Get().allocateCommandBuffersUnique(allocInfo);

		std::vector<std::unique_ptr<ICommandBuffer>> results;
		uint32_t index = 0;
		for (vk::UniqueCommandBuffer& commandBuffer : commandBuffers)
		{
			std::string uniqueName = std::format("{}{}", name, index);
			vkDevice.SetResourceName(ResourceType::CommandBuffer, commandBuffer.get(), uniqueName.c_str());
			results.emplace_back(std::make_unique<CommandBuffer>(std::move(commandBuffer)));
			++index;
		}

		return results;
	}

	std::unique_ptr<ICommandBuffer> CommandPool::BeginResourceCommandBuffer(const IDevice& device) const
	{
		const Device& vkDevice = static_cast<const Device&>(device);
		vk::CommandBufferAllocateInfo allocInfo(m_commandPool.get(), vk::CommandBufferLevel::ePrimary, 1);
		std::vector<vk::UniqueCommandBuffer> commandBuffers = vkDevice.Get().allocateCommandBuffersUnique(allocInfo);
		vk::UniqueCommandBuffer commandBuffer = std::move(commandBuffers.front());

		vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
		commandBuffer->begin(beginInfo);
		return std::make_unique<CommandBuffer>(std::move(commandBuffer));
	}

	void CommandPool::Reset(const IDevice& device) const
	{
		vk::Device deviceImp = static_cast<const Device&>(device).Get();
		deviceImp.resetCommandPool(m_commandPool.get());
	}
}