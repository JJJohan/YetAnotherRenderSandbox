#pragma once

#include <vulkan/vulkan.hpp>
#include "SwapChainSupportDetails.hpp"
#include "../Resources/ISwapChain.hpp"

struct VmaAllocator_T;
typedef struct VmaAllocator_T* VmaAllocator;

namespace Engine::OS
{
	class Window;
}

namespace Engine::Rendering
{
	class IDevice;
	class IPhysicalDevice;
}

namespace Engine::Rendering::Vulkan
{
	class Surface;

	class SwapChain : public ISwapChain
	{
	public:
		SwapChain();

		inline const vk::SwapchainKHR& Get() const { return m_swapChain.get(); }

		bool Initialise(const IPhysicalDevice& physicalDevice, const IDevice& device, const Surface& surface,
			const Engine::OS::Window& window, VmaAllocator allocator, const glm::uvec2& size, bool hdr);

		static SwapChainSupportDetails QuerySwapChainSupport(const vk::PhysicalDevice& physicalDevice, const Surface& surface);

	private:
		vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats, bool hdr);

		vk::UniqueSwapchainKHR m_swapChain;
	};
}