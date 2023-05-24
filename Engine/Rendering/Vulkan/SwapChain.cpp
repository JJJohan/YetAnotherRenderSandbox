#include "SwapChain.hpp"
#include "Device.hpp"
#include "PhysicalDevice.hpp"
#include "Surface.hpp"
#include "ImageView.hpp"
#include "RenderImage.hpp"
#include "OS/Window.hpp"
#include "Core/Logging/Logger.hpp"

using namespace Engine::Logging;
using namespace Engine::OS;

namespace Engine::Rendering::Vulkan
{
	SwapChain::SwapChain()
		: m_swapChain(nullptr)
		, m_swapChainImageFormat(vk::Format::eUndefined)
		, m_depthImageFormat(vk::Format::eUndefined)
		, m_swapChainExtent()
		, m_swapChainImages()
		, m_swapChainImageViews()
		, m_depthImage(nullptr)
		, m_depthImageView(nullptr)
		, m_colorImage(nullptr)
		, m_colorImageView(nullptr)
		, m_sampleCount(vk::SampleCountFlagBits::e1)
		, m_hdrSupport()
	{
	}

	const vk::SwapchainKHR& SwapChain::Get() const
	{
		return m_swapChain.get();
	}

	vk::SampleCountFlagBits SwapChain::GetSampleCount() const
	{
		return m_sampleCount;
	}

	bool SwapChain::IsHDRCapable() const
	{
		return m_hdrSupport.has_value() && m_hdrSupport.value();
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

	vk::PresentModeKHR ChooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes)
	{
		return vk::PresentModeKHR::eFifo;
	}

	bool SwapChain::CreateColorImage(const PhysicalDevice& physicalDevice, const Device& device, VmaAllocator allocator, vk::SampleCountFlagBits samples)
	{
		m_colorImage = std::make_unique<RenderImage>(allocator);
		vk::Extent3D extent(m_swapChainExtent.width, m_swapChainExtent.height, 1);
		if (!m_colorImage->Initialise(vk::ImageType::e2D, m_swapChainImageFormat, extent, samples, 1, vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment,
			VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0, vk::SharingMode::eExclusive))
		{
			Logger::Error("Failed to create color image.");
			return false;
		}

		m_colorImageView = std::make_unique<ImageView>();
		if (!m_colorImageView->Initialise(device, m_colorImage->Get(), 1, m_swapChainImageFormat, vk::ImageAspectFlagBits::eColor))
		{
			Logger::Error("Failed to create color image view.");
			return false;
		}

		return true;
	}

	bool SwapChain::CreateDepthImage(const PhysicalDevice& physicalDevice, const Device& device, VmaAllocator allocator, vk::SampleCountFlagBits samples)
	{
		m_depthImageFormat = physicalDevice.FindDepthFormat();
		if (m_depthImageFormat == vk::Format::eUndefined)
		{
			Logger::Error("Failed to find suitable format for depth texture.");
			return false;
		}

		m_depthImage = std::make_unique<RenderImage>(allocator);
		vk::Extent3D extent(m_swapChainExtent.width, m_swapChainExtent.height, 1);
		if (!m_depthImage->Initialise(vk::ImageType::e2D, m_depthImageFormat, extent, samples, 1, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment,
			VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0, vk::SharingMode::eExclusive))
		{
			Logger::Error("Failed to create depth image.");
			return false;
		}

		m_depthImageView = std::make_unique<ImageView>();
		if (!m_depthImageView->Initialise(device, m_depthImage->Get(), 1, m_depthImageFormat, vk::ImageAspectFlagBits::eDepth))
		{
			Logger::Error("Failed to create depth image view.");
			return false;
		}

		return true;
	}

	RenderImage& SwapChain::GetColorImage() const
	{
		return *m_colorImage;
	}

	RenderImage& SwapChain::GetDepthImage() const
	{
		return *m_depthImage;
	}

	std::vector<RenderImage>& SwapChain::GetSwapChainImages()
	{
		return m_swapChainImages;
	}

	const std::vector<std::unique_ptr<ImageView>>& SwapChain::GetSwapChainImageViews() const
	{
		return m_swapChainImageViews;
	}

	const ImageView& SwapChain::GetColorView() const
	{
		return *m_colorImageView;
	}

	const ImageView& SwapChain::GetDepthView() const
	{
		return *m_depthImageView;
	}

	vk::Extent2D ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, const glm::uvec2& size)
	{
		if (capabilities.currentExtent.width != UINT32_MAX)
		{
			return capabilities.currentExtent;
		}
		else
		{
			vk::Extent2D actualExtent(size.x, size.y);

			actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

			return actualExtent;
		}
	}

	const vk::Format& SwapChain::GetFormat() const
	{
		return m_swapChainImageFormat;
	}

	const vk::Format& SwapChain::GetDepthFormat() const
	{
		return m_depthImageFormat;
	}

	const vk::Extent2D& SwapChain::GetExtent() const
	{
		return m_swapChainExtent;
	}

	bool SwapChain::Initialise(const PhysicalDevice& physicalDevice, const Device& device, const Surface& surface,
		const Window& window, VmaAllocator allocator, const glm::uvec2& size, vk::SampleCountFlagBits sampleCount, bool hdr)
	{
		SwapChainSupportDetails swapChainSupportDetails = QuerySwapChainSupport(physicalDevice.Get(), surface);

		vk::SurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupportDetails.Formats, hdr);
		vk::PresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupportDetails.PresentModes);
		m_swapChainExtent = ChooseSwapExtent(swapChainSupportDetails.Capabilities, size);
		m_swapChainImageFormat = surfaceFormat.format;
		m_sampleCount = sampleCount;

		uint32_t imageCount = swapChainSupportDetails.Capabilities.minImageCount + 1;
		if (swapChainSupportDetails.Capabilities.maxImageCount > 0 && imageCount > swapChainSupportDetails.Capabilities.maxImageCount)
		{
			imageCount = swapChainSupportDetails.Capabilities.maxImageCount;
		}

		vk::UniqueSwapchainKHR oldSwapChain = std::move(m_swapChain);

		vk::SwapchainCreateInfoKHR createInfo{};
		createInfo.surface = surface.Get();
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = m_swapChainExtent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
		createInfo.preTransform = swapChainSupportDetails.Capabilities.currentTransform;
		createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = oldSwapChain.get();

		QueueFamilyIndices indices = physicalDevice.GetQueueFamilyIndices();
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

		const vk::Device& deviceImp = device.Get();

		VkSwapchainKHR swapChain{};
		m_swapChain = deviceImp.createSwapchainKHRUnique(createInfo);
		if (!m_swapChain.get())
		{
			Logger::Error("Failed to create swap chain.");
			return false;
		}

		oldSwapChain.reset();
		m_swapChainImages.clear();
		m_swapChainImageViews.clear();
		m_depthImageView.reset();
		m_depthImage.reset();
		m_colorImageView.reset();
		m_colorImage.reset();

		std::vector<vk::Image> images = deviceImp.getSwapchainImagesKHR(m_swapChain.get());
		if (images.empty())
		{
			Logger::Error("Failed to enumerate swap chain images.");
			return false;
		}

		m_swapChainImages.reserve(imageCount);
		m_swapChainImageViews.reserve(imageCount);
		for (const auto& image : images)
		{
			std::unique_ptr<ImageView> imageView = std::make_unique<ImageView>();
			if (!imageView->Initialise(device, image, 1, surfaceFormat.format, vk::ImageAspectFlagBits::eColor))
			{
				Logger::Error("Failed to create image view for swap chain image.");
				return false;
			}

			m_swapChainImages.push_back(RenderImage(image, surfaceFormat.format));
			m_swapChainImageViews.push_back(std::move(imageView));
		}

		if (sampleCount != vk::SampleCountFlagBits::e1 && !CreateColorImage(physicalDevice, device, allocator, sampleCount))
		{
			return false;
		}

		if (!CreateDepthImage(physicalDevice, device, allocator, sampleCount))
		{
			return false;
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