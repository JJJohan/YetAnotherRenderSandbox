#include "Framebuffer.hpp"
#include "Core/Logging/Logger.hpp"
#include "Device.hpp"
#include "RenderPass.hpp"
#include "ImageView.hpp"

using namespace Engine::Logging;

namespace Engine::Rendering::Vulkan
{
	Framebuffer::Framebuffer()
		: m_framebuffer(nullptr)
	{

	}

	VkFramebuffer Framebuffer::Get() const
	{
		return m_framebuffer;
	}

	void Framebuffer::Shutdown(const Device& device)
	{
		if (m_framebuffer)
		{
			vkDestroyFramebuffer(device.Get(), m_framebuffer, nullptr);
			m_framebuffer = nullptr;
		}
	}

	bool Framebuffer::CreateFramebuffer(const Device& device, const VkExtent2D& swapChainExtent, const RenderPass& renderPass, const ImageView& imageView)
	{
		VkImageView imageViewImp = imageView.Get();

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass.Get();
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = &imageViewImp;
		framebufferInfo.width = swapChainExtent.width;
		framebufferInfo.height = swapChainExtent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(device.Get(), &framebufferInfo, nullptr, &m_framebuffer) != VK_SUCCESS)
		{
			Logger::Error("Failed to create framebuffer.");
			return false;
		}

		return true;
	}
}