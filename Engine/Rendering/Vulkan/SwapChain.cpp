#include "SwapChain.hpp"
#include "Device.hpp"
#include "PhysicalDevice.hpp"
#include "Surface.hpp"
#include "RenderPass.hpp"
#include "ImageView.hpp"
#include "RenderImage.hpp"
#include "Framebuffer.hpp"
#include "Core/Logging/Logger.hpp"

using namespace Engine::Logging;
using namespace Engine::OS;

namespace Engine::Rendering::Vulkan
{
	SwapChain::SwapChain()
		: m_swapChain(nullptr)
		, m_swapChainImageFormat(vk::Format::eUndefined)
		, m_swapChainExtent()
		, m_swapChainImageViews()
		, m_framebuffers()
		, m_depthImage(nullptr)
		, m_depthImageView(nullptr)
	{
	}

	const vk::SwapchainKHR& SwapChain::Get() const
	{
		return m_swapChain.get();
	}

	void SwapChain::DestroyFramebuffers(const Device& device)
	{
		m_framebuffers.clear();
	}

	vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
	{
		auto match = std::find_if(availableFormats.begin(), availableFormats.end(), [](const auto& availableFormat)
			{
				return availableFormat.format == vk::Format::eR8G8B8Srgb && availableFormat.colorSpace == vk::ColorSpaceKHR::eExtendedSrgbNonlinearEXT;
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

	bool SwapChain::CreateDepthImage(const PhysicalDevice& physicalDevice, const Device& device, VmaAllocator allocator)
	{
		vk::Format depthFormat = physicalDevice.FindDepthFormat();
		if (depthFormat == vk::Format::eUndefined)
		{
			Logger::Error("Failed to find suitable format for depth texture.");
			return false;
		}

		m_depthImage = std::make_unique<RenderImage>(allocator);
		vk::Extent3D extent(m_swapChainExtent.width, m_swapChainExtent.height, 1);
		if (!m_depthImage->Initialise(vk::ImageType::e2D, depthFormat, extent, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment,
			VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0, vk::SharingMode::eExclusive))
		{
			Logger::Error("Failed to create depth image.");
			return false;
		}

		m_depthImageView = std::make_unique<ImageView>();
		if (!m_depthImageView->Initialise(device, m_depthImage->Get(), depthFormat, vk::ImageAspectFlagBits::eDepth))
		{
			Logger::Error("Failed to create depth image view.");
			return false;
		}

		return true;
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

	const vk::Extent2D& SwapChain::GetExtent() const
	{
		return m_swapChainExtent;
	}

	std::vector<Framebuffer*> SwapChain::GetFramebuffers() const
	{
		std::vector<Framebuffer*> frameBuffers;
		frameBuffers.reserve(m_framebuffers.size());
		for (const auto& it : m_framebuffers)
		{
			frameBuffers.emplace_back(std::addressof(*it));
		}
		return frameBuffers;
	}

	bool SwapChain::Initialise(const PhysicalDevice& physicalDevice, const Device& device, const Surface& surface, VmaAllocator allocator, const glm::uvec2& size)
	{
		std::optional<SwapChainSupportDetails> supportResult = QuerySwapChainSupport(physicalDevice.Get(), surface);
		if (!supportResult.has_value())
		{
			return false;
		}

		SwapChainSupportDetails swapChainSupportDetails = supportResult.value();

		vk::SurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupportDetails.Formats);
		vk::PresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupportDetails.PresentModes);
		m_swapChainExtent = ChooseSwapExtent(swapChainSupportDetails.Capabilities, size);
		m_swapChainImageFormat = surfaceFormat.format;

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
		m_swapChainImageViews.clear();
		m_depthImageView.reset();
		m_depthImage.reset();

		std::vector<vk::Image> images = deviceImp.getSwapchainImagesKHR(m_swapChain.get());
		if (images.empty())
		{
			Logger::Error("Failed to enumerate swap chain images.");
			return false;
		}

		m_swapChainImageViews.reserve(imageCount);
		for (const auto& image : images)
		{
			std::unique_ptr<ImageView> imageView = std::make_unique<ImageView>();
			if (!imageView->Initialise(device, image, surfaceFormat.format, vk::ImageAspectFlagBits::eColor))
			{
				Logger::Error("Failed to create image view for swap chain image.");
				return false;
			}

			m_swapChainImageViews.push_back(std::move(imageView));
		}

		if (!CreateDepthImage(physicalDevice, device, allocator))
		{
			return false;
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

	bool SwapChain::CreateFramebuffers(const Device& device, const RenderPass& renderPass)
	{
		VkDevice deviceImp = device.Get();
		m_framebuffers.clear();
		m_framebuffers.reserve(m_swapChainImageViews.size());
		for (const auto& imageView : m_swapChainImageViews)
		{
			std::unique_ptr<Framebuffer> framebuffer = std::make_unique<Framebuffer>();
			if (!framebuffer->Initialise(device, m_swapChainExtent, renderPass, *imageView, *m_depthImageView))
			{
				return false;
			}

			m_framebuffers.push_back(std::move(framebuffer));
		}
		return true;
	}
}