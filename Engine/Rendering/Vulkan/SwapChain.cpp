#include "SwapChain.hpp"
#include "Device.hpp"
#include "PhysicalDevice.hpp"
#include "Surface.hpp"
#include "ImageView.hpp"
#include "OS/Window.hpp"
#include "Core/Logger.hpp"
#include "VulkanTypesInterop.hpp"
#include "RenderImage.hpp"

using namespace Engine::OS;

namespace Engine::Rendering::Vulkan
{
	SwapChain::SwapChain()
		: m_swapChain(nullptr)
	{
	}

	vk::SurfaceFormatKHR SwapChain::ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats, bool hdr)
	{
		if (!m_hdrSupport.has_value() || hdr)
		{
			auto hdrMatch = std::find_if(availableFormats.begin(), availableFormats.end(), [](const auto& availableFormat)
				{
					return availableFormat.format == vk::Format::eA2B10G10R10UnormPack32 && availableFormat.colorSpace != vk::ColorSpaceKHR::eSrgbNonlinear
						&& availableFormat.colorSpace != vk::ColorSpaceKHR::eExtendedSrgbNonlinearEXT;
				});

			if (hdrMatch != availableFormats.end())
			{
				m_hdrSupport = true;
				if (hdr)
					return *hdrMatch;
			}
		}

		auto match = std::find_if(availableFormats.begin(), availableFormats.end(), [](const auto& availableFormat)
			{
				return availableFormat.format == vk::Format::eR8G8B8Srgb && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
			});

		if (match != availableFormats.end())
		{
			return *match;
		}

		return availableFormats.front();
	}


	glm::uvec2 ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, const glm::uvec2& size)
	{
		if (capabilities.currentExtent.width != UINT32_MAX)
		{
			return glm::uvec2(capabilities.currentExtent.width, capabilities.currentExtent.height);
		}
		else
		{
			glm::uvec2 result;
			result.x = std::clamp(size.x, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			result.y = std::clamp(size.y, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
			return result;
		}
	}

	vk::PresentModeKHR ChooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes)
	{
		return vk::PresentModeKHR::eFifo;
	}

	bool SwapChain::Initialise(const IPhysicalDevice& physicalDevice, const IDevice& device, const Surface& surface,
		const Window& window, VmaAllocator allocator, const glm::uvec2& size, bool hdr)
	{
		const Device& vkDevice = static_cast<const Device&>(device);
		const PhysicalDevice& vkPhysicalDevice = static_cast<const PhysicalDevice&>(physicalDevice);

		SwapChainSupportDetails swapChainSupportDetails = QuerySwapChainSupport(vkPhysicalDevice.Get(), surface);

		vk::SurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupportDetails.Formats, hdr);
		vk::PresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupportDetails.PresentModes);
		m_swapChainExtent = ChooseSwapExtent(swapChainSupportDetails.Capabilities, size);
		m_swapChainImageFormat = FromVulkanFormat(surfaceFormat.format);

		uint32_t imageCount = swapChainSupportDetails.Capabilities.minImageCount + 1;
		if (swapChainSupportDetails.Capabilities.maxImageCount > 0 && imageCount > swapChainSupportDetails.Capabilities.maxImageCount)
		{
			imageCount = swapChainSupportDetails.Capabilities.maxImageCount;
		}

		vk::UniqueSwapchainKHR oldSwapChain = std::move(m_swapChain);

		ImageUsageFlags usageFlags = ImageUsageFlags::ColorAttachment | ImageUsageFlags::TransferDst;

		vk::SwapchainCreateInfoKHR createInfo{};
		createInfo.surface = surface.Get();
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = vk::Extent2D(m_swapChainExtent.x, m_swapChainExtent.y);
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = static_cast<vk::ImageUsageFlagBits>(usageFlags);
		createInfo.preTransform = swapChainSupportDetails.Capabilities.currentTransform;
		createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = oldSwapChain.get();

		QueueFamilyIndices indices = vkPhysicalDevice.GetQueueFamilyIndices();
		uint32_t queueFamilyIndices[] = { indices.GraphicsFamily.value(), indices.PresentFamily.value() };

		if (indices.GraphicsFamily != indices.PresentFamily)
		{
			createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
		{
			createInfo.imageSharingMode = vk::SharingMode::eExclusive;
			createInfo.queueFamilyIndexCount = 0; // Optional
			createInfo.pQueueFamilyIndices = nullptr; // Optional
		}

		const vk::Device& deviceImp = vkDevice.Get();

		VkSwapchainKHR swapChain{};
		m_swapChain = deviceImp.createSwapchainKHRUnique(createInfo);
		if (!m_swapChain.get())
		{
			Logger::Error("Failed to create swap chain.");
			return false;
		}

		oldSwapChain.reset();
		m_swapChainImages.clear();

		std::vector<vk::Image> images = deviceImp.getSwapchainImagesKHR(m_swapChain.get());
		if (images.empty())
		{
			Logger::Error("Failed to enumerate swap chain images.");
			return false;
		}

		m_swapChainImages.reserve(imageCount);
		for (const auto& image : images)
		{
			std::unique_ptr<RenderImage> renderImage = std::make_unique<RenderImage>(image, surfaceFormat.format, usageFlags);
			if (!renderImage->InitialiseView("SwapchainImage", device, ImageAspectFlags::Color))
			{
				return false;
			}

			m_swapChainImages.push_back(std::move(renderImage));
		}

		if (hdr)
		{
			// Not sure if helpful, doesn't appear to affect output.
			MonitorInfo monitorInfo;
			if (window.QueryMonitorInfo(monitorInfo))
			{
				vk::HdrMetadataEXT metadata{};
				metadata.displayPrimaryRed = vk::XYColorEXT(monitorInfo.RedPrimary[0], monitorInfo.RedPrimary[1]);
				metadata.displayPrimaryGreen = vk::XYColorEXT(monitorInfo.GreenPrimary[0], monitorInfo.GreenPrimary[1]);
				metadata.displayPrimaryBlue = vk::XYColorEXT(monitorInfo.BluePrimary[0], monitorInfo.BluePrimary[1]);
				metadata.whitePoint = vk::XYColorEXT(monitorInfo.WhitePoint[0], monitorInfo.WhitePoint[1]);
				metadata.maxContentLightLevel = 0;
				metadata.maxFrameAverageLightLevel = 0;
				metadata.minLuminance = monitorInfo.MinLuminance;
				metadata.maxLuminance = monitorInfo.MaxLuminance;

				deviceImp.setHdrMetadataEXT(1, &m_swapChain.get(), &metadata);
			}
		}

		return true;
	}

	SwapChainSupportDetails SwapChain::QuerySwapChainSupport(const vk::PhysicalDevice& physicalDevice, const Surface& surface)
	{
		const vk::SurfaceKHR& surfaceImp = surface.Get();

		SwapChainSupportDetails details{};
		details.Capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surfaceImp);
		details.Formats = physicalDevice.getSurfaceFormatsKHR(surfaceImp);
		details.PresentModes = physicalDevice.getSurfacePresentModesKHR(surfaceImp);

		return details;
	}
}