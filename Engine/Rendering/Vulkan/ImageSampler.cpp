#include "ImageSampler.hpp"
#include "Core/Logging/Logger.hpp"
#include "Device.hpp"

using namespace Engine::Logging;

namespace Engine::Rendering::Vulkan
{
	ImageSampler::ImageSampler()
		: m_sampler(nullptr)
	{
	}

	const vk::Sampler& ImageSampler::Get() const
	{
		return m_sampler.get();
	}

	bool ImageSampler::Initialise(const Device& device, vk::Filter magFilter, vk::Filter minFilter, vk::SamplerMipmapMode mipMapMode, 
		vk::SamplerAddressMode addressMode, float maxAnisotropy)
	{
		vk::SamplerCreateInfo createInfo(vk::SamplerCreateFlags(), magFilter, minFilter, mipMapMode, addressMode,
			addressMode, addressMode, 0.0f, maxAnisotropy > 0.0f, maxAnisotropy);

		m_sampler = device.Get().createSamplerUnique(createInfo);
		if (!m_sampler.get())
		{
			Logger::Error("Failed to create image sampler.");
			return false;
		}

		return true;
	}
}