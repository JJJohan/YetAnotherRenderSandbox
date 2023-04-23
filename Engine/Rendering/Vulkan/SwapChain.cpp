#include "SwapChain.hpp"
#include "Device.hpp"
#include "PhysicalDevice.hpp"
#include "Surface.hpp"
#include "RenderPass.hpp"
#include "Core/Logging/Logger.hpp"
#include "OS/Window.hpp"

using namespace Engine::Logging;
using namespace Engine::OS;

namespace Engine::Rendering::Vulkan
{
	SwapChain::SwapChain()
		: m_swapChain(nullptr)
		, m_swapChainImageFormat(VK_FORMAT_UNDEFINED)
		, m_swapChainExtent()
		, m_swapChainImageViews()
		, m_framebuffers()
	{
	}

	VkSwapchainKHR SwapChain::Get() const
	{
		return m_swapChain;
	}

	void SwapChain::ShutdownFramebuffers(const Device& device)
	{
		for (auto& framebuffer : m_framebuffers)
		{
			framebuffer.Shutdown(device);
		}
		m_framebuffers.clear();
	}

	void SwapChain::Shutdown(const Device& device)
	{
		ShutdownFramebuffers(device);

		for (auto& imageView : m_swapChainImageViews)
		{
			imageView.Shutdown(device);
		}
		m_swapChainImageViews.clear();

		if (m_swapChain != nullptr)
		{
			vkDestroySwapchainKHR(device.Get(), m_swapChain, nullptr);
			m_swapChain = nullptr;
		}
	}

	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
	{
		auto match = std::find_if(availableFormats.begin(), availableFormats.end(), [](const auto& availableFormat)
			{
				return availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
			});

		if (match != availableFormats.end())
		{
			return *match;
		}

		return availableFormats.front();
	}

	VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
	{
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, const Window& window)
	{
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return capabilities.currentExtent;
		}
		else
		{
			VkExtent2D actualExtent = { window.GetWidth(), window.GetHeight() };

			actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

			return actualExtent;
		}
	}

	VkFormat SwapChain::GetFormat() const
	{
		return m_swapChainImageFormat;
	}

	VkExtent2D SwapChain::GetExtent() const
	{
		return m_swapChainExtent;
	}

	const std::vector<Framebuffer>& SwapChain::GetFramebuffers() const
	{
		return m_framebuffers;
	}

	bool SwapChain::CreateSwapChain(const PhysicalDevice& physicalDevice, const Device& device, const Surface& surface, const Window& window)
	{
		std::optional<SwapChainSupportDetails> supportResult = QuerySwapChainSupport(physicalDevice.Get(), surface);
		if (!supportResult.has_value())
		{
			return false;
		}

		SwapChainSupportDetails swapChainSupportDetails = supportResult.value();

		VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupportDetails.Formats);
		VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupportDetails.PresentModes);
		VkExtent2D extent = ChooseSwapExtent(swapChainSupportDetails.Capabilities, window);

		uint32_t imageCount = swapChainSupportDetails.Capabilities.minImageCount + 1;
		if (swapChainSupportDetails.Capabilities.maxImageCount > 0 && imageCount > swapChainSupportDetails.Capabilities.maxImageCount)
		{
			imageCount = swapChainSupportDetails.Capabilities.maxImageCount;
		}

		VkSwapchainKHR oldSwapChain = m_swapChain;

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = surface.Get();
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		createInfo.preTransform = swapChainSupportDetails.Capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = oldSwapChain;

		QueueFamilyIndices indices = physicalDevice.GetQueueFamilyIndices();
		uint32_t queueFamilyIndices[] = { indices.GraphicsFamily.value(), indices.PresentFamily.value() };

		if (indices.GraphicsFamily != indices.PresentFamily) 
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else 
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0; // Optional
			createInfo.pQueueFamilyIndices = nullptr; // Optional
		}

		VkDevice deviceImp = device.Get();

		VkSwapchainKHR swapChain{};
		if (vkCreateSwapchainKHR(deviceImp, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
		{
			Logger::Error("Failed to create swap chain.");
			return false;
		}

		if (oldSwapChain != nullptr)
		{
			Shutdown(device);
		}
		m_swapChain = swapChain;

		VkResult res = vkGetSwapchainImagesKHR(deviceImp, m_swapChain, &imageCount, nullptr);
		if (res != VK_SUCCESS)
		{
			Logger::Error("Failed to enumerate swap chain images.");
			Shutdown(device);
			return false;
		}

		std::vector<VkImage> images(imageCount);
		res = vkGetSwapchainImagesKHR(deviceImp, m_swapChain, &imageCount, images.data());
		if (res != VK_SUCCESS)
		{
			Logger::Error("Failed to enumerate swap chain images.");
			Shutdown(device);
			return false;
		}

		m_swapChainImageViews.reserve(imageCount);
		for (const auto& image : images)
		{
			ImageView imageView{};
			if (!imageView.CreateImageView(device, image, surfaceFormat.format, VK_IMAGE_ASPECT_COLOR_BIT))
			{
				Logger::Error("Failed to create image view for swap chain image.");
				Shutdown(device);
				return false;
			}

			m_swapChainImageViews.push_back(imageView);
		}

		m_swapChainImageFormat = surfaceFormat.format;
		m_swapChainExtent = extent;

		return true;
	}

	std::optional<SwapChainSupportDetails> SwapChain::QuerySwapChainSupport(const VkPhysicalDevice& physicalDevice, const Surface& surface)
	{
		SwapChainSupportDetails details{};

		VkSurfaceKHR surfaceImp = surface.Get();

		VkResult res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surfaceImp, &details.Capabilities);
		if (res != VK_SUCCESS)
		{
			Logger::Error("Failed to query physical device surface capabilities. Error code: {}", static_cast<int>(res));
			return {};
		}

		uint32_t formatCount;
		res = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surfaceImp, &formatCount, nullptr);
		if (res != VK_SUCCESS)
		{
			Logger::Error("Failed to enumerate physical device surface formats. Error code: {}", static_cast<int>(res));
			return {};
		}

		if (formatCount != 0)
		{
			details.Formats.resize(formatCount);
			res = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surfaceImp, &formatCount, details.Formats.data());
			if (res != VK_SUCCESS)
			{
				Logger::Error("Failed to enumerate physical device surface formats. Error code: {}", static_cast<int>(res));
				return {};
			}
		}

		uint32_t presentModeCount;
		res = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surfaceImp, &presentModeCount, nullptr);
		if (res != VK_SUCCESS)
		{
			Logger::Error("Failed to enumerate physical device surface present modes. Error code: {}", static_cast<int>(res));
			return {};
		}

		if (presentModeCount != 0)
		{
			details.PresentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surfaceImp, &presentModeCount, details.PresentModes.data());
			if (res != VK_SUCCESS)
			{
				Logger::Error("Failed to enumerate physical device surface present modes. Error code: {}", static_cast<int>(res));
				return {};
			}
		}

		return details;
	}

	bool SwapChain::CreateFramebuffers(const Device& device, const RenderPass& renderPass)
	{
		VkDevice deviceImp = device.Get();
		m_framebuffers.reserve(m_swapChainImageViews.size());
		for (const auto& imageView : m_swapChainImageViews)
		{
			Framebuffer framebuffer{};
			if (!framebuffer.CreateFramebuffer(device, m_swapChainExtent, renderPass, imageView))
			{
				return false;
			}

			m_framebuffers.push_back(framebuffer);
		}
		return true;
	}
}