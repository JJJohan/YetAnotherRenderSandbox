#pragma once

#include <vulkan/vulkan.hpp>
#include <vector>

namespace Engine::Rendering::Vulkan
{
	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR Capabilities;
		std::vector<VkSurfaceFormatKHR> Formats;
		std::vector<VkPresentModeKHR> PresentModes;
	};
}