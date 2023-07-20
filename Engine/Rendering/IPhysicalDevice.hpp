#pragma once

#include "Types.hpp"

namespace Engine::Rendering
{
	struct QueueFamilyIndices;

	class IPhysicalDevice
	{
	public:
		IPhysicalDevice()
			: m_depthFormat(Format::Undefined)
		{
		}

		virtual ~IPhysicalDevice() = default;

		inline Format GetDepthFormat() const { return m_depthFormat; }
		virtual bool FormatSupported(Format format) const = 0;
		virtual float GetMaxAnisotropy() const = 0;
		virtual bool SupportsBCTextureCompression() const = 0;
		virtual const QueueFamilyIndices& GetQueueFamilyIndices() const = 0;
		virtual Format FindDepthFormat() = 0;

	protected:
		Format m_depthFormat;
	};
}