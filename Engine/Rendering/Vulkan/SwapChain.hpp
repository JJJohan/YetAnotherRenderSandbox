#pragma once

#include <vulkan/vulkan.hpp>
#include "SwapChainSupportDetails.hpp"
#include <glm/glm.hpp>

struct VmaAllocator_T;
typedef struct VmaAllocator_T* VmaAllocator;

namespace Engine::Rendering::Vulkan
{
	class Device;
	class PhysicalDevice;
	class Surface;
	class RenderPass;
	class Framebuffer;
	class ImageView;
	class RenderImage;

	class SwapChain
	{
	public:
		SwapChain();
		void DestroyFramebuffers(const Device& device);

		const vk::Format& GetFormat() const;
		const vk::Extent2D& GetExtent() const;
		std::vector<Framebuffer*> GetFramebuffers() const;
		const vk::SwapchainKHR& Get() const;
		vk::SampleCountFlagBits GetSampleCount() const;

		bool Initialise(const PhysicalDevice& physicalDevice, const Device& device, const Surface& surface,
			VmaAllocator allocator, const glm::uvec2& size, vk::SampleCountFlagBits sampleCount);
		bool CreateFramebuffers(const Device& device, const RenderPass& renderPass);

		static SwapChainSupportDetails QuerySwapChainSupport(const vk::PhysicalDevice& physicalDevice, const Surface& surface);

	private:
		bool CreateColorImage(const PhysicalDevice& physicalDevice, const Device& device, VmaAllocator allocator, vk::SampleCountFlagBits samples);
		bool CreateDepthImage(const PhysicalDevice& physicalDevice, const Device& device, VmaAllocator allocator, vk::SampleCountFlagBits samples);

		vk::SampleCountFlagBits m_sampleCount;
		vk::UniqueSwapchainKHR m_swapChain;
		vk::Format m_swapChainImageFormat;
		vk::Extent2D m_swapChainExtent;
		std::vector<std::unique_ptr<ImageView>> m_swapChainImageViews;
		std::vector<std::unique_ptr<Framebuffer>> m_framebuffers;
		std::unique_ptr<RenderImage> m_depthImage;
		std::unique_ptr<ImageView> m_depthImageView;
		std::unique_ptr<RenderImage> m_colorImage;
		std::unique_ptr<ImageView> m_colorImageView;
	};
}