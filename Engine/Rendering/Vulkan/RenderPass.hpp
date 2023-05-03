#pragma once

#include <vulkan/vulkan.hpp>

namespace Engine::Rendering::Vulkan
{
	class Device;
	class PhysicalDevice;
	class SwapChain;

	class RenderPass
	{
	public:
		RenderPass();
		const vk::RenderPass& Get() const;
		bool Initialise(const PhysicalDevice& physicalDevice, const Device& device, const SwapChain& swapChain, vk::SampleCountFlagBits sampleCount);
		vk::SampleCountFlagBits GetSampleCount() const;

	private:
		vk::UniqueRenderPass m_renderPass;
		vk::SampleCountFlagBits m_sampleCount;
	};
}