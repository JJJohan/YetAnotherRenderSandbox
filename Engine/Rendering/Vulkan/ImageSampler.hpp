#pragma once

#include <vulkan/vulkan.hpp>

namespace Engine::Rendering::Vulkan
{
	class Device;

	class ImageSampler
	{
	public:
		ImageSampler();
		inline const vk::Sampler& Get() const { return m_sampler.get(); }
		bool Initialise(const Device& device, vk::Filter magFilter, vk::Filter minFilter, vk::SamplerMipmapMode mipMapMode,
			vk::SamplerAddressMode addressMode, float maxAnisotropy);

	private:
		vk::UniqueSampler m_sampler;
	};
}