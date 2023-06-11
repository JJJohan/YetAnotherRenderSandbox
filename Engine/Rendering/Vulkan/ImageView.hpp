#pragma once

#include <vulkan/vulkan.hpp>

namespace Engine::Rendering::Vulkan
{
	class Device;

	class ImageView
	{
	public:
		ImageView();
		inline const vk::ImageView& Get() const { return m_imageView.get(); }
		bool Initialise(const Device& device, const vk::Image& image, uint32_t mipLevels, vk::Format format, vk::ImageAspectFlags aspectFlags);

	private:
		vk::UniqueImageView m_imageView;
	};
}