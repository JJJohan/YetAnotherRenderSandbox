#pragma once

#include <vulkan/vulkan.hpp>
#include "SwapChainSupportDetails.hpp"
#include <glm/glm.hpp>
#include <optional>

struct VmaAllocator_T;
typedef struct VmaAllocator_T* VmaAllocator;

namespace Engine::OS
{
	class Window;
}

namespace Engine::Rendering::Vulkan
{
	class Device;
	class PhysicalDevice;
	class Surface;
	class ImageView;
	class RenderImage;

	class SwapChain
	{
	public:
		SwapChain();

		const vk::Format& GetFormat() const;
		const vk::Format& GetDepthFormat() const;
		const vk::Extent2D& GetExtent() const;
		const vk::SwapchainKHR& Get() const;

		RenderImage& GetDepthImage() const;
		std::vector<RenderImage>& GetSwapChainImages();
		const std::vector<std::unique_ptr<ImageView>>& GetSwapChainImageViews() const;
		const ImageView& GetDepthView() const;
		bool IsHDRCapable() const;

		bool Initialise(const PhysicalDevice& physicalDevice, const Device& device, const Surface& surface, 
			const Engine::OS::Window& window, VmaAllocator allocator, const glm::uvec2& size, bool hdr);

		static SwapChainSupportDetails QuerySwapChainSupport(const vk::PhysicalDevice& physicalDevice, const Surface& surface);

	private:
		bool CreateDepthImage(const PhysicalDevice& physicalDevice, const Device& device, VmaAllocator allocator);
		vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats, bool hdr);

		vk::UniqueSwapchainKHR m_swapChain;
		vk::Format m_swapChainImageFormat;
		vk::Format m_depthImageFormat;
		vk::Extent2D m_swapChainExtent;
		std::vector<RenderImage> m_swapChainImages;
		std::vector<std::unique_ptr<ImageView>> m_swapChainImageViews;
		std::unique_ptr<RenderImage> m_depthImage;
		std::unique_ptr<ImageView> m_depthImageView;
		std::optional<bool> m_hdrSupport;
	};
}