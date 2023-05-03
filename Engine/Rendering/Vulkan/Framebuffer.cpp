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

	const vk::Framebuffer& Framebuffer::Get() const
	{
		return m_framebuffer.get();
	}

	bool Framebuffer::Initialise(const Device& device, const vk::Extent2D& swapChainExtent, const RenderPass& renderPass,
		const ImageView& imageView, const ImageView& depthImageView, const ImageView& colorImageView)
	{
		std::vector<vk::ImageView> attachments;
		attachments.reserve(3);
		if (renderPass.GetSampleCount() != vk::SampleCountFlagBits::e1)
		{
			attachments.push_back(colorImageView.Get());
			attachments.push_back(depthImageView.Get());
			attachments.push_back(imageView.Get());
		}
		else
		{
			attachments.push_back(imageView.Get());
			attachments.push_back(depthImageView.Get());
		}

		vk::FramebufferCreateInfo framebufferInfo(vk::FramebufferCreateFlags(), renderPass.Get(), static_cast<uint32_t>(attachments.size()), attachments.data(), swapChainExtent.width, swapChainExtent.height, 1);

		m_framebuffer = device.Get().createFramebufferUnique(framebufferInfo);
		if (!m_framebuffer.get())
		{
			Logger::Error("Failed to create framebuffer.");
			return false;
		}

		return true;
	}
}