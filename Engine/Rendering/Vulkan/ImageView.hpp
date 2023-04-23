#pragma once

#include <vulkan/vulkan.hpp>

namespace Engine::Rendering::Vulkan
{
	class Device;

	class ImageView
	{
	public:
		ImageView();
		VkImageView Get() const;
		bool CreateImageView(const Device& device, const VkImage& image, VkFormat format, VkImageAspectFlags aspectFlags);
		void Shutdown(const Device& device);

	private:
		VkImageView m_imageView;
	};
}