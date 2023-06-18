#include "ImageView.hpp"
#include "Core/Logging/Logger.hpp"
#include "Device.hpp"
#include "RenderImage.hpp"
#include "VulkanTypesInterop.hpp"

using namespace Engine::Logging;

namespace Engine::Rendering::Vulkan
{
	ImageView::ImageView()
		: m_imageView(nullptr)
	{
	}

	bool ImageView::Initialise(const IDevice& device, const IRenderImage& image, uint32_t mipLevels, Format format, ImageAspectFlags aspectFlags)
	{
		vk::ImageSubresourceRange subResourceRange(GetImageAspectFlags(aspectFlags), 0, mipLevels, 0, 1);

		vk::ImageViewCreateInfo createInfo(vk::ImageViewCreateFlags(), static_cast<const RenderImage&>(image).Get(), vk::ImageViewType::e2D, GetVulkanFormat(format));
		createInfo.setSubresourceRange(subResourceRange);

		m_imageView = static_cast<const Device&>(device).Get().createImageViewUnique(createInfo);
		if (!m_imageView.get())
		{
			Logger::Error("Failed to create image view.");
			return false;
		}

		return true;
	}
}