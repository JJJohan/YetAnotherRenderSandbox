#pragma once

#include <vulkan/vulkan.hpp>
#include "../Resources/IImageSampler.hpp"

namespace Engine::Rendering::Vulkan
{
	class ImageSampler : public IImageSampler
	{
	public:
		ImageSampler();
		inline const vk::Sampler& Get() const { return m_sampler.get(); }
		virtual bool Initialise(const IDevice& device, Filter magFilter, Filter minFilter, SamplerMipmapMode mipMapMode,
			SamplerAddressMode addressMode, float maxAnisotropy) override;

	private:
		vk::UniqueSampler m_sampler;
	};
}