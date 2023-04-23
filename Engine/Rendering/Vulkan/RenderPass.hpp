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
		void Shutdown(const Device& device);
		VkRenderPass Get() const;
		bool CreateRenderPass(const Device& device, const SwapChain& swapChain);

	private:
		VkRenderPass m_renderPass;
	};
}