#include "VulkanRenderStats.hpp"
#include "Device.hpp"
#include "PhysicalDevice.hpp"
#include "GBuffer.hpp"
#include "Core/Logging/Logger.hpp"

using namespace Engine::Logging;

namespace Engine::Rendering::Vulkan
{
	VulkanRenderStats::VulkanRenderStats(const GBuffer& gBuffer)
		: RenderStats()
		, m_statisticsQueryPools()
		, m_availableStatisticsQueryPools()
		, m_usedStatisticsQueryPools()
		, m_timestampQueryPools()
		, m_availableTimestampQueryPools()
		, m_usedTimestampQueryPools()
		, m_timestampSupported(false)
		, m_statisticsSupported(false)
		, m_gBuffer(gBuffer)
		, m_statisticsQueryCount(4)
		, m_usedQueryCount(0)
		, m_timestampPeriod(0.0f)
	{
	}

	bool VulkanRenderStats::Initialise(const PhysicalDevice& physicalDevice, const Device& device)
	{
		// Check if the selected device supports timestamps. A value of zero means no support.
		const vk::PhysicalDeviceLimits& limits = physicalDevice.GetLimits();
		m_timestampPeriod = limits.timestampPeriod;
		m_timestampSupported = m_timestampPeriod != 0.0f && limits.timestampComputeAndGraphics;
		if (!m_timestampSupported)
		{
			Logger::Warning("Timestamp statistics not supported, statistics will be limited.");
		}

		m_statisticsSupported = physicalDevice.GetFeatures().pipelineStatisticsQuery;
		if (!m_statisticsSupported)
		{
			Logger::Warning("Pipeline statistics not supported, statistics will be limited.");
		}

		return true;
	}

	bool VulkanRenderStats::Begin(const Device& device, const vk::CommandBuffer& commandBuffer)
	{
		++m_usedQueryCount;

		if (m_timestampSupported)
		{
			vk::QueryPool timestampQueryPool;
			if (m_availableTimestampQueryPools.empty())
			{
				vk::QueryPoolCreateInfo queryPoolInfo{};
				queryPoolInfo.queryType = vk::QueryType::eTimestamp;

				queryPoolInfo.queryCount = 2;
				vk::UniqueQueryPool& newQueryPool = m_timestampQueryPools.emplace_back(device.Get().createQueryPoolUnique(queryPoolInfo));
				if (!newQueryPool.get())
				{
					return false;
				}

				timestampQueryPool = newQueryPool.get();
			}
			else
			{
				timestampQueryPool = m_availableTimestampQueryPools.top();
				m_availableTimestampQueryPools.pop();
			}

			commandBuffer.resetQueryPool(timestampQueryPool, 0, 2);
			commandBuffer.writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe, timestampQueryPool, 0);
			m_usedTimestampQueryPools.push_back(timestampQueryPool);
		}

		if (m_statisticsSupported)
		{
			vk::QueryPool statisticsQueryPool;
			if (m_availableStatisticsQueryPools.empty())
			{
				vk::QueryPoolCreateInfo queryPoolInfo{};
				queryPoolInfo.queryType = vk::QueryType::ePipelineStatistics;
				queryPoolInfo.pipelineStatistics =
					vk::QueryPipelineStatisticFlagBits::eInputAssemblyVertices |
					vk::QueryPipelineStatisticFlagBits::eInputAssemblyPrimitives |
					vk::QueryPipelineStatisticFlagBits::eVertexShaderInvocations |
					vk::QueryPipelineStatisticFlagBits::eFragmentShaderInvocations;

				queryPoolInfo.queryCount = m_statisticsQueryCount;
				vk::UniqueQueryPool& newQueryPool = m_statisticsQueryPools.emplace_back(device.Get().createQueryPoolUnique(queryPoolInfo));
				if (!newQueryPool.get())
				{
					return false;
				}

				statisticsQueryPool = newQueryPool.get();
			}
			else
			{
				statisticsQueryPool = m_availableStatisticsQueryPools.top();
				m_availableStatisticsQueryPools.pop();
			}

			commandBuffer.resetQueryPool(statisticsQueryPool, 0, m_statisticsQueryCount);
			commandBuffer.beginQuery(statisticsQueryPool, 0, vk::QueryControlFlags());
			m_usedStatisticsQueryPools.push_back(statisticsQueryPool);
		}

		return true;
	}

	void VulkanRenderStats::End(const vk::CommandBuffer& commandBuffer)
	{
		if (m_timestampSupported)
			commandBuffer.writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, m_usedTimestampQueryPools.back(), 1);
		if (m_statisticsSupported)
			commandBuffer.endQuery(m_usedStatisticsQueryPools.back(), 0);
	}

	void VulkanRenderStats::GetResults(const Device& device)
	{
		uint32_t usedQueryCount = m_usedQueryCount;
		m_usedQueryCount = 0;
		if (!m_timestampSupported && !m_statisticsSupported)
			return;

		m_statsData.clear();
		m_statsData.resize(static_cast<size_t>(usedQueryCount) + 1);
		const vk::Device& deviceImp = device.Get();

		for (uint32_t i = 0; i < usedQueryCount; ++i)
		{
			RenderStatsData& data = m_statsData[i];
			std::vector<uint64_t> buffer(static_cast<size_t>(m_statisticsQueryCount) + 1);

			if (m_timestampSupported)
			{
				const vk::QueryPool& timestampQueryPool = m_usedTimestampQueryPools[i];
				vk::Result result = deviceImp.getQueryPoolResults(timestampQueryPool, 0, 2, sizeof(uint64_t) * 3,
					buffer.data(), sizeof(uint64_t), vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWithAvailability);

				if (result == vk::Result::eSuccess && buffer[2] != 0)
				{
					data.RenderTime = float(buffer[1] - buffer[0]) * m_timestampPeriod / 1000000.0f;
				}

				m_availableTimestampQueryPools.push(timestampQueryPool);
			}

			if (m_statisticsSupported)
			{
				const vk::QueryPool& statisticsQueryPool = m_usedStatisticsQueryPools[i];
				vk::Result result = deviceImp.getQueryPoolResults(statisticsQueryPool, 0, 1, m_statisticsQueryCount * sizeof(uint64_t) + sizeof(uint64_t),
					buffer.data(), sizeof(uint64_t), vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWithAvailability);

				if (result == vk::Result::eSuccess && buffer[m_statisticsQueryCount] != 0)
				{
					data.InputAssemblyVertexCount = buffer[0];
					data.InputAssemblyPrimitivesCount = buffer[1];
					data.VertexShaderInvocations = buffer[2];
					data.FragmentShaderInvocations = buffer[3];
				}

				m_availableStatisticsQueryPools.push(statisticsQueryPool);
			}
		}

		// The final entry contains the totals.
		RenderStatsData& total = m_statsData[usedQueryCount];
		for (size_t i = 0; i < usedQueryCount; ++i)
		{
			RenderStatsData& data = m_statsData[i];
			total.InputAssemblyVertexCount += data.InputAssemblyVertexCount;
			total.InputAssemblyPrimitivesCount += data.InputAssemblyPrimitivesCount;
			total.VertexShaderInvocations += data.VertexShaderInvocations;
			total.FragmentShaderInvocations += data.FragmentShaderInvocations;
			total.RenderTime += data.RenderTime;
		}

		m_usedTimestampQueryPools.clear();
		m_usedStatisticsQueryPools.clear();
	}
}