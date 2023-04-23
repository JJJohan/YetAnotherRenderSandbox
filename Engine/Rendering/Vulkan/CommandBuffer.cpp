#include "CommandBuffer.hpp"
#include "Core/Logging/Logger.hpp"
#include "Device.hpp"

using namespace Engine::Logging;

namespace Engine::Rendering::Vulkan
{
	CommandBuffer::CommandBuffer()
		: m_commandBuffer(nullptr)
	{

	}

	VkCommandBuffer CommandBuffer::Get() const
	{
		return m_commandBuffer;
	}

	void CommandBuffer::Shutdown(const Device& device)
	{
		if (m_commandPool)
		{
			vkDestroyCommandPool(device.Get(), m_commandPool, nullptr);
			m_commandPool = nullptr;
		}
	}

	bool CommandBuffer::CreateCommandBuffer(const Device& device)
	{
		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass.Get();
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = &imageViewImp;
		framebufferInfo.width = swapChainExtent.width;
		framebufferInfo.height = swapChainExtent.height;
		framebufferInfo.layers = 1;

		if (vkCreateCommandPool(device.Get(), &commandPoolInfo, nullptr, &m_commandPool) != VK_SUCCESS)
		{
			Logger::Error("Failed to create command pool.");
			return false;
		}

		return true;
	}
}