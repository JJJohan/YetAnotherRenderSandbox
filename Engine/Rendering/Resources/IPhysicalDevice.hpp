#pragma once

#include "../Types.hpp"

namespace Engine::Rendering
{
	class IPhysicalDevice
	{
	public:
		IPhysicalDevice() = default;
		virtual ~IPhysicalDevice() = default;

		virtual Format FindDepthFormat() const = 0;
		virtual bool FormatSupported(Format format) const = 0;
		virtual float GetMaxAnisotropy() const = 0;
		virtual bool SupportsBCTextureCompression() const = 0;
	};
}