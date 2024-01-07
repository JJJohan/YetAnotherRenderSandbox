#pragma once

#include "../Types.hpp"

namespace Engine::Rendering
{
	class IDevice;

	enum class SamplerCreationFlags
	{
		None,
		ReductionSampler
	};

	class IImageSampler
	{
	public:
		IImageSampler() = default;
		virtual ~IImageSampler() = default;

		virtual bool Initialise(const IDevice& device, Filter magFilter, Filter minFilter, SamplerMipmapMode mipmapMode,
			SamplerAddressMode addressMode, float maxAnisotropy, SamplerCreationFlags flags = SamplerCreationFlags::None) = 0;
	};
}