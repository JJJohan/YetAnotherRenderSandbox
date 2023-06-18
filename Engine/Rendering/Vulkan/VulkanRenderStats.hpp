#pragma once

#include <vulkan/vulkan.hpp>
#include "../RenderStats.hpp"

namespace Engine::Rendering
{
	class GBuffer;
	class ShadowMap;
	class ICommandBuffer;
}

namespace Engine::Rendering::Vulkan
{
	class Device;
	class PhysicalDevice;

	class VulkanRenderStats : public RenderStats
	{
	public:
		VulkanRenderStats(const GBuffer& gBuffer, const ShadowMap& shadowMap);
		bool Initialise(const PhysicalDevice& physicalDevice, const Device& device, uint32_t renderPassCount);
		void Begin(const ICommandBuffer& commandBuffer);
		void End(const ICommandBuffer& commandBuffer);
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