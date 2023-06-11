#pragma once

#include <vulkan/vulkan.hpp>
#include "SwapChainSupportDetails.hpp"
#include "RenderImage.hpp"
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

	class SwapChain
	{
	public:
		SwapChain();

		inline const vk::Format& GetFormat() const { return m_swapChainImageFormat; }
		inline const vk::Extent2D& GetExtent() const { return m_swapChainExtent; }
		inline const vk::SwapchainKHR& Get() const { return m_swapChain.get(); }

		inline RenderImage& GetSwapChainImage(uint32_t imageIndex) { return m_swapChainImages[imageIndex]; }
		inline const ImageView& GetSwapChainImageView(uint32_t imageIndex) const { return *m_swapChainImageViews[imageIndex]; }
		inline bool IsHDRCapable() const { return m_hdrSupport.has_value() && m_hdrSupport.value(); }

		bool Initialise(const PhysicalDevice& physicalDevice, const Device& device, const Surface& surface,
			const Engine::OS::Window& window, VmaAllocator allocator, const glm::uvec2& size, bool hdr);

		static SwapChainSupportDetails QuerySwapChainSupport(const vk::PhysicalDevice& physicalDevice, const Surface& surface);

	private:
		vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats, bool hdr);

		vk::UniqueSwapchainKHR m_swapChain;
		vk::Format m_swapChainImageFormat;
		vk::Extent2D m_swapChainExtent;
		std::vector<RenderImage> m_swapChainImages;
		std::vector<std::unique_ptr<ImageView>> m_swapChainImageViews;
		std::optional<bool> m_hdrSupport;
	};
}