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
		const vk::Framebuffer& Get() const;
		bool Initialise(const Device& device, const vk::Extent2D& swapChainExtent, const RenderPass& renderPass, const ImageView& imageView);

	private:
		vk::UniqueFramebuffer m_framebuffer;
	};
}