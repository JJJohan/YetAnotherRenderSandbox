#pragma once

#include <vulkan/vulkan.hpp>

namespace Engine::Rendering::Vulkan
{
	class Device;
	class RenderPass;
	class ImageView;

	class Framebuffer
	{
	public:
		Framebuffer();
		void Shutdown(const Device& device);
		VkFramebuffer Get() const;
		bool CreateFramebuffer(const Device& device, const VkExtent2D& swapChainExtent, const RenderPass& renderPass, const ImageView& imageView);

	private:
		VkFramebuffer m_framebuffer;
	};
}