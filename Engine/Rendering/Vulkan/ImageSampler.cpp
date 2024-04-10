#include "ImageSampler.hpp"
#include "Core/Logging/Logger.hpp"
#include "Device.hpp"
#include "VulkanTypesInterop.hpp"

using namespace Engine::Logging;

namespace Engine::Rendering::Vulkan
{
	ImageSampler::ImageSampler()
		: m_sampler(nullptr)
	{
	}

	bool ImageSampler::Initialise(const IDevice& device, Filter magFilter, Filter minFilter, SamplerMipmapMode mipMapMode,
		SamplerAddressMode addressMode, float maxAnisotropy, SamplerCreationFlags flags)
	{
		const vk::SamplerAddressMode& vulkanAddressMode = GetSamplerAddressMode(addressMode);
		vk::SamplerCreateInfo createInfo(vk::SamplerCreateFlags(), GetFilter(magFilter), GetFilter(minFilter),
			GetSamplerMipmapMode(mipMapMode), vulkanAddressMode,
			vulkanAddressMode, vulkanAddressMode, 0.0f, maxAnisotropy > 0.0f, maxAnisotropy);
		createInfo.borderColor = vk::BorderColor::eFloatOpaqueWhite;
		createInfo.maxLod = VK_LOD_CLAMP_NONE;

		vk::SamplerReductionModeCreateInfo createInfoReduction(vk::SamplerReductionMode::eMax);
		if (flags == SamplerCreationFlags::ReductionSampler)
		{
			createInfo.pNext = &createInfoReduction;
		}

		m_sampler = static_cast<const Device&>(device).Get().createSamplerUnique(createInfo);
		if (!m_sampler.get())
		{
			Logger::Error("Failed to create image sampler.");
			return false;
		}

		return true;
	}
}