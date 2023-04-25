#pragma once

#include <vulkan/vulkan.hpp>

namespace Engine::Rendering::Vulkan
{
	class Device;
	class SwapChain;

	class RenderPass
	{
	public:
		RenderPass();
		const vk::RenderPass& Get() const;
		bool Initialise(const Device& device, const SwapChain& swapChain);

	private:
		vk::UniqueRenderPass m_renderPass;
	};
}