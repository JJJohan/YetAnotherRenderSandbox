#pragma once

#include <stdint.h>
#include "../Types.hpp"

namespace Engine::Rendering
{
	class IDevice;
	class IRenderImage;

	class IImageView
	{
	public:
		IImageView() = default;
		virtual ~IImageView() = default;
		virtual bool Initialise(const IDevice& device, const IRenderImage& image, uint32_t mipLevels,
			Format format, ImageAspectFlags aspectFlags) = 0;
	};
}