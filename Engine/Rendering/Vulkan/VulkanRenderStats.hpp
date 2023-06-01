#pragma once

#include <vulkan/vulkan.hpp>
#include "../RenderStats.hpp"

namespace Engine::Rendering::Vulkan
{
	class Device;
	class PhysicalDevice;
	class GBuffer;
	class ShadowMap;

	class VulkanRenderStats : public RenderStats
	{
	public:
		VulkanRenderStats(const GBuffer& gBuffer, const ShadowMap& shadowMap);
		bool Initialise(const PhysicalDevice& physicalDevice, const Device& device, uint32_t renderPassCount);
		void Begin(const vk::CommandBuffer& commandBuffer);
		void End(const vk::CommandBuffer& commandBuffer);
		void GetResults(const PhysicalDevice& physicalDevice, const Device& device);

	private:
		vk::UniqueQueryPool m_statisticsQueryPool;
		vk::UniqueQueryPool m_timestampQueryPool;

		const GBuffer& m_gBuffer;
		const ShadowMap& m_shadowMap;
		bool m_timestampSupported;
		bool m_statisticsSupported;
		float m_timestampPeriod;
		uint32_t m_renderPassCount;
		uint32_t m_renderPassIndex;
	};
}