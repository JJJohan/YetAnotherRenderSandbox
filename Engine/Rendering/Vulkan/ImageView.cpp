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

	bool ImageView::Initialise(const char* name, const IDevice& device, const IRenderImage& image, uint32_t baseMipLevel, uint32_t mipLevels, 
		uint32_t layerCount, Format format, ImageAspectFlags aspectFlags)
	{
		m_mipLevels = mipLevels;
		m_layerCount = layerCount;

		const Device& deviceImp = static_cast<const Device&>(device);
		vk::ImageSubresourceRange subResourceRange(GetImageAspectFlags(aspectFlags), baseMipLevel, mipLevels, 0, layerCount);

		vk::ImageViewType type = layerCount == 1 ? vk::ImageViewType::e2D : vk::ImageViewType::e2DArray;
		vk::ImageViewCreateInfo createInfo(vk::ImageViewCreateFlags(), static_cast<const RenderImage&>(image).Get(), type, GetVulkanFormat(format));
		createInfo.setSubresourceRange(subResourceRange);

		m_imageView = deviceImp.Get().createImageViewUnique(createInfo);
		if (!m_imageView.get())
		{
			Logger::Error("Failed to create image view.");
			return false;
		}

		deviceImp.SetResourceName(ResourceType::ImageView, m_imageView.get(), name);

		return true;
	}
}