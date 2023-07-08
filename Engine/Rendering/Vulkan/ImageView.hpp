#pragma once

#include <vulkan/vulkan.hpp>
#include "../Resources/IImageView.hpp"

namespace Engine::Rendering::Vulkan
{
	class ImageView : public IImageView
	{
	public:
		ImageView();
		inline const vk::ImageView& Get() const { return m_imageView.get(); }
		virtual bool Initialise(const IDevice& device, const IRenderImage& image,
			uint32_t mipLevels, uint32_t layerCount, Format format, ImageAspectFlags aspectFlags) override;

	private:
		vk::UniqueImageView m_imageView;
	};
}