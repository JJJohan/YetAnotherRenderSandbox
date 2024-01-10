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
		IImageView()
			: m_mipLevels(1)
			, m_layerCount(1)
		{
		}

		virtual ~IImageView() = default;
		virtual bool Initialise(const char* name, const IDevice& device, const IRenderImage& image, uint32_t baseMipLevel,
			uint32_t mipLevels, uint32_t layerCount, Format format, ImageAspectFlags aspectFlags) = 0;

		inline uint32_t GetMipLevels() const { return m_mipLevels; }
		inline uint32_t GetLayerCount() const { return m_layerCount; }

	protected:
		uint32_t m_mipLevels;
		uint32_t m_layerCount;
	};
}