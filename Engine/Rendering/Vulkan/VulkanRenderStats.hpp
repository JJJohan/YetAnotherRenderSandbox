#pragma once

#include <vulkan/vulkan.hpp>
#include <vector>
#include <stack>
#include "../RenderStats.hpp"

namespace Engine::Rendering::Vulkan
{
	class Device;
	class PhysicalDevice;
	class GBuffer;

	class VulkanRenderStats : public RenderStats
	{
	public:
		VulkanRenderStats(const GBuffer& gBuffer);
		bool Initialise(const PhysicalDevice& physicalDevice, const Device& device);
		bool Begin(const Device& device, const vk::CommandBuffer& commandBuffer);
		void End(const vk::CommandBuffer& commandBuffer);
		void GetResults(const Device& device);

	private:
		std::vector<vk::UniqueQueryPool> m_statisticsQueryPools;
		std::stack<vk::QueryPool> m_availableStatisticsQueryPools;
		std::vector<vk::QueryPool> m_usedStatisticsQueryPools;

		std::vector<vk::UniqueQueryPool> m_timestampQueryPools;
		std::stack<vk::QueryPool> m_availableTimestampQueryPools;
		std::vector<vk::QueryPool> m_usedTimestampQueryPools;

		const GBuffer& m_gBuffer;
		bool m_timestampSupported;
		bool m_statisticsSupported;
		uint32_t m_usedQueryCount;
		uint32_t m_statisticsQueryCount;
		float m_timestampPeriod;
	};
}