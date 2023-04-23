#pragma once

#include <vulkan/vulkan.hpp>
#include "SwapChainSupportDetails.hpp"
#include "ImageView.hpp"
#include "Framebuffer.hpp"

namespace Engine::OS
{
	class Window;
}

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
		void Shutdown(const Device& device);
		void ShutdownFramebuffers(const Device& device);

		VkFormat GetFormat() const;
		VkExtent2D GetExtent() const;
		const std::vector<Framebuffer>& GetFramebuffers() const;
		VkSwapchainKHR Get() const;

		bool CreateSwapChain(const PhysicalDevice& physicalDevice, const Device& device, const Surface& surface, const Engine::OS::Window& window);
		bool CreateFramebuffers(const Device& device, const RenderPass& renderPass);

		static std::optional<SwapChainSupportDetails> QuerySwapChainSupport(const VkPhysicalDevice& physicalDevice, const Surface& surface);

	private:
		VkSwapchainKHR m_swapChain;
		VkFormat m_swapChainImageFormat;
		VkExtent2D m_swapChainExtent;
		std::vector<ImageView> m_swapChainImageViews;
		std::vector<Framebuffer> m_framebuffers;
	};
}