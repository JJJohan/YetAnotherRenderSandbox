#pragma once

#include <vulkan/vulkan.hpp>

namespace Engine::Rendering::Vulkan
{
	class Device;

	class ImageView
	{
	public:
		ImageView();
		const vk::ImageView& Get() const;
		bool Initialise(const Device& device, const vk::Image& image, uint32_t mipLevels, vk::Format format, vk::ImageAspectFlags aspectFlags);

	private:
		vk::UniqueImageView m_imageView;
	};
}