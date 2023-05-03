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

	const vk::ImageView& ImageView::Get() const
	{
		return m_imageView.get();
	}

	bool ImageView::Initialise(const Device& device, const vk::Image& image, uint32_t mipLevels, vk::Format format, vk::ImageAspectFlags aspectFlags)
	{
		vk::ImageSubresourceRange subResourceRange(aspectFlags, 0, mipLevels, 0, 1);

		vk::ImageViewCreateInfo createInfo(vk::ImageViewCreateFlags(), image, vk::ImageViewType::e2D, format);
		createInfo.setSubresourceRange(subResourceRange);

		m_imageView = device.Get().createImageViewUnique(createInfo);
		if (!m_imageView.get())
		{
			Logger::Error("Failed to create image view.");
			return false;
		}

		return true;
	}
}