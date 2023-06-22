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
	class VulkanRenderStats : public RenderStats
	{
	public:
		VulkanRenderStats(const GBuffer& gBuffer, const ShadowMap& shadowMap);
		virtual bool Initialise(const IPhysicalDevice& physicalDevice, const IDevice& device, uint32_t renderPassCount) override;
		virtual void Begin(const ICommandBuffer& commandBuffer) override;
		virtual void End(const ICommandBuffer& commandBuffer) override;
		virtual void FinaliseResults(const IPhysicalDevice& physicalDevice, const IDevice& device) override;

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