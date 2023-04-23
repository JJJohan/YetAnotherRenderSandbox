#include "ImageView.hpp"
#include "Core/Logging/Logger.hpp"
#include "Device.hpp"

using namespace Engine::Logging;

namespace Engine::Rendering::Vulkan
{
	ImageView::ImageView()
		: m_imageView(nullptr)
	{

	}

	VkImageView ImageView::Get() const
	{
		return m_imageView;
	}

	bool ImageView::CreateImageView(const Device& device, const VkImage& image, VkFormat format, VkImageAspectFlags aspectFlags)
	{
		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = image;
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = format;

		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(device.Get(), &createInfo, nullptr, &m_imageView) != VK_SUCCESS)
		{
			Logger::Error("Failed to create image view.");
			return false;
		}

		return true;
	}

	void ImageView::Shutdown(const Device& device)
	{
		if (m_imageView)
		{
			vkDestroyImageView(device.Get(), m_imageView, nullptr);
			m_imageView = nullptr;
		}
	}
}