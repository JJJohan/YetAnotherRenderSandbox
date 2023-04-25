#pragma once

#include <vulkan/vulkan.hpp>
#include "SwapChainSupportDetails.hpp"
#include "ImageView.hpp"
#include "Framebuffer.hpp"
#include <glm/glm.hpp>

namespace Engine::Rendering::Vulkan
{
	class Device;
	class PhysicalDevice;
	class Surface;
	class RenderPass;

	class SwapChain
	{
	public:
		SwapChain();
		void DestroyFramebuffers(const Device& device);

		const vk::Format& GetFormat() const;
		const vk::Extent2D& GetExtent() const;
		std::vector<Framebuffer*> GetFramebuffers() const;
		const vk::SwapchainKHR& Get() const;

		bool Initialise(const PhysicalDevice& physicalDevice, const Device& device, const Surface& surface, const glm::uvec2& size);
		bool CreateFramebuffers(const Device& device, const RenderPass& renderPass);

		static SwapChainSupportDetails QuerySwapChainSupport(const vk::PhysicalDevice& physicalDevice, const Surface& surface);

	private:
		vk::UniqueSwapchainKHR m_swapChain;
		vk::Format m_swapChainImageFormat;
		vk::Extent2D m_swapChainExtent;
		std::vector<std::unique_ptr<ImageView>> m_swapChainImageViews;
		std::vector<std::unique_ptr<Framebuffer>> m_framebuffers;
	};
}