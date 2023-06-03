#include "VulkanRenderStats.hpp"
#include "Device.hpp"
#include "PhysicalDevice.hpp"
#include "GBuffer.hpp"
#include "ShadowMap.hpp"
#include "Core/Logging/Logger.hpp"

using namespace Engine::Logging;

namespace Engine::Rendering::Vulkan
{
	const int statisticsCount = 4;

	VulkanRenderStats::VulkanRenderStats(const GBuffer& gBuffer, const ShadowMap& shadowMap)
		: RenderStats()
		, m_statisticsQueryPool()
		, m_timestampQueryPool()
		, m_timestampSupported(false)
		, m_statisticsSupported(false)
		, m_gBuffer(gBuffer)
		, m_shadowMap(shadowMap)
		, m_timestampPeriod(0.0f)
		, m_renderPassCount(0)
		, m_renderPassIndex(0)
	{
	}

	bool VulkanRenderStats::Initialise(const PhysicalDevice& physicalDevice, const Device& device, uint32_t renderPassCount)
	{
		m_renderPassCount = renderPassCount;

		const vk::PhysicalDeviceLimits& limits = physicalDevice.GetLimits();

		m_timestampPeriod = limits.timestampPeriod;
		m_timestampSupported = m_timestampPeriod != 0.0f && limits.timestampComputeAndGraphics;
		if (m_timestampSupported)
		{
			vk::QueryPoolCreateInfo queryPoolInfo{};
			queryPoolInfo.queryType = vk::QueryType::eTimestamp;

			queryPoolInfo.queryCount = m_renderPassCount * 2;
			m_timestampQueryPool = device.Get().createQueryPoolUnique(queryPoolInfo);
			if (!m_timestampQueryPool.get())
			{
				return false;
			}
		}
		else
		{
			Logger::Warning("Timestamp statistics not supported, statistics will be limited.");
		}

		m_statisticsSupported = physicalDevice.GetFeatures().pipelineStatisticsQuery;
		if (m_statisticsSupported)
		{
			vk::QueryPoolCreateInfo queryPoolInfo{};
			queryPoolInfo.queryType = vk::QueryType::ePipelineStatistics;
			queryPoolInfo.pipelineStatistics =
				vk::QueryPipelineStatisticFlagBits::eInputAssemblyVertices |
				vk::QueryPipelineStatisticFlagBits::eInputAssemblyPrimitives |
				vk::QueryPipelineStatisticFlagBits::eVertexShaderInvocations |
				vk::QueryPipelineStatisticFlagBits::eFragmentShaderInvocations;

			queryPoolInfo.queryCount = m_renderPassCount * statisticsCount;
			m_statisticsQueryPool = device.Get().createQueryPoolUnique(queryPoolInfo);
			if (!m_statisticsQueryPool.get())
			{
				return false;
			}
		}
		else
		{
			Logger::Warning("Pipeline statistics not supported, statistics will be limited.");
		}

		return true;
	}

	void VulkanRenderStats::Begin(const vk::CommandBuffer& commandBuffer)
	{
		if (m_timestampSupported)
		{
			commandBuffer.resetQueryPool(m_timestampQueryPool.get(), m_renderPassIndex * 2, 2);
			commandBuffer.writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe, m_timestampQueryPool.get(), m_renderPassIndex * 2);
		}

		if (m_statisticsSupported)
		{
			commandBuffer.resetQueryPool(m_statisticsQueryPool.get(), m_renderPassIndex, 1);
			commandBuffer.beginQuery(m_statisticsQueryPool.get(), m_renderPassIndex, vk::QueryControlFlags());
		}
	}

	void VulkanRenderStats::End(const vk::CommandBuffer& commandBuffer)
	{
		if (m_timestampSupported)
			commandBuffer.writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, m_timestampQueryPool.get(), m_renderPassIndex * 2 + 1);

		if (m_statisticsSupported)
			commandBuffer.endQuery(m_statisticsQueryPool.get(), m_renderPassIndex);

		++m_renderPassIndex;
	}

	void VulkanRenderStats::GetResults(const PhysicalDevice& physicalDevice, const Device& device)
	{
		memset(&m_memoryStats, 0, sizeof(MemoryStats));

		m_memoryStats.GBuffer = m_gBuffer.GetMemoryUsage();
		m_memoryStats.ShadowMap = m_shadowMap.GetMemoryUsage();
		vk::PhysicalDeviceMemoryBudgetPropertiesEXT budgetProperties{};
		vk::PhysicalDeviceMemoryProperties2 memoryProperties{};
		memoryProperties.pNext = &budgetProperties;
		physicalDevice.Get().getMemoryProperties2(&memoryProperties);
		uint32_t heapCount = memoryProperties.memoryProperties.memoryHeapCount;

		for (uint32_t i = 0; i < heapCount; ++i)
		{
			// Split memory usage into dedicated and shared based on DEVICE_LOCAL flag.
			if ((memoryProperties.memoryProperties.memoryHeaps[i].flags & vk::MemoryHeapFlagBits::eDeviceLocal) == vk::MemoryHeapFlagBits::eDeviceLocal)
			{
				m_memoryStats.DedicatedUsage += budgetProperties.heapUsage[i];
				m_memoryStats.DedicatedBudget += budgetProperties.heapBudget[i];
			}
			else
			{
				m_memoryStats.SharedUsage += budgetProperties.heapUsage[i];
				m_memoryStats.SharedBudget += budgetProperties.heapBudget[i];
			}

		}

		if (!m_timestampSupported && !m_statisticsSupported)
			return;

		m_renderPassIndex = 0;
		m_statsData.clear();
		m_statsData.resize(static_cast<size_t>(m_renderPassCount) + 1);
		const vk::Device& deviceImp = device.Get();


		for (uint32_t i = 0; i < m_renderPassCount; ++i)
		{
			FrameStats& data = m_statsData[i];
			std::vector<uint64_t> buffer(static_cast<size_t>(statisticsCount) + 1);

			if (m_timestampSupported)
			{
				vk::Result result = deviceImp.getQueryPoolResults(m_timestampQueryPool.get(), i * 2, 2, sizeof(uint64_t) * 3,
					buffer.data(), sizeof(uint64_t), vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWithAvailability);

				if (result == vk::Result::eSuccess && buffer[2] != 0)
				{
					data.RenderTime = float(buffer[1] - buffer[0]) * m_timestampPeriod / 1000000.0f;
				}
			}

			if (m_statisticsSupported)
			{
				vk::Result result = deviceImp.getQueryPoolResults(m_statisticsQueryPool.get(), i, 1, statisticsCount * sizeof(uint64_t) + sizeof(uint64_t),
					buffer.data(), sizeof(uint64_t), vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWithAvailability);

				if (result == vk::Result::eSuccess && buffer[statisticsCount] != 0)
				{
					data.InputAssemblyVertexCount = buffer[0];
					data.InputAssemblyPrimitivesCount = buffer[1];
					data.VertexShaderInvocations = buffer[2];
					data.FragmentShaderInvocations = buffer[3];
				}
			}
		}

		// The final entry contains the totals.
		FrameStats& total = m_statsData[m_renderPassCount];
		for (size_t i = 0; i < m_renderPassCount; ++i)
		{
			FrameStats& data = m_statsData[i];
			total.InputAssemblyVertexCount += data.InputAssemblyVertexCount;
			total.InputAssemblyPrimitivesCount += data.InputAssemblyPrimitivesCount;
			total.VertexShaderInvocations += data.VertexShaderInvocations;
			total.FragmentShaderInvocations += data.FragmentShaderInvocations;
			total.RenderTime += data.RenderTime;
		}
	}
}