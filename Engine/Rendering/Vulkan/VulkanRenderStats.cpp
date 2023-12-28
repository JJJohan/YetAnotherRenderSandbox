#include "VulkanRenderStats.hpp"
#include "Device.hpp"
#include "PhysicalDevice.hpp"
#include "Core/Logging/Logger.hpp"
#include "CommandBuffer.hpp"
#include "../RenderResources/IRenderResource.hpp"

using namespace Engine::Logging;

namespace Engine::Rendering::Vulkan
{
	const int statisticsCount = 4;

	VulkanRenderStats::VulkanRenderStats()
		: RenderStats()
		, m_statisticsQueryPool()
		, m_timestampQueryPool()
		, m_timestampSupported(false)
		, m_statisticsSupported(false)
		, m_timestampPeriod(0.0f)
		, m_renderPassCount(0)
		, m_renderPassIndex(0)
		, m_renderPassNames()
	{
	}

	bool VulkanRenderStats::Initialise(const IPhysicalDevice& physicalDevice, const IDevice& device, uint32_t renderPassCount)
	{
		m_renderPassCount = renderPassCount;

		const Device& vkDevice = static_cast<const Device&>(device);
		const PhysicalDevice& vkPhysicalDevice = static_cast<const PhysicalDevice&>(physicalDevice);
		const vk::Device& deviceImp = vkDevice.Get();

		const vk::PhysicalDeviceLimits& limits = vkPhysicalDevice.GetLimits();

		m_timestampPeriod = limits.timestampPeriod;
		m_timestampSupported = m_timestampPeriod != 0.0f && limits.timestampComputeAndGraphics;
		if (m_timestampSupported)
		{
			vk::QueryPoolCreateInfo queryPoolInfo{};
			queryPoolInfo.queryType = vk::QueryType::eTimestamp;

			queryPoolInfo.queryCount = m_renderPassCount * 2;
			m_timestampQueryPool = deviceImp.createQueryPoolUnique(queryPoolInfo);
			if (!m_timestampQueryPool.get())
			{
				return false;
			}

			deviceImp.resetQueryPool(m_timestampQueryPool.get(), 0, m_renderPassCount * 2);
		}
		else
		{
			Logger::Warning("Timestamp statistics not supported, statistics will be limited.");
		}

		m_statisticsSupported = vkPhysicalDevice.GetFeatures().pipelineStatisticsQuery;
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
			m_statisticsQueryPool = deviceImp.createQueryPoolUnique(queryPoolInfo);
			if (!m_statisticsQueryPool.get())
			{
				return false;
			}

			deviceImp.resetQueryPool(m_statisticsQueryPool.get(), 0, m_renderPassCount * statisticsCount);
		}
		else
		{
			Logger::Warning("Pipeline statistics not supported, statistics will be limited.");
		}

		return true;
	}

	void VulkanRenderStats::Begin(const ICommandBuffer& commandBuffer, const char* passName)
	{
		const vk::CommandBuffer& vkCommandBuffer = static_cast<const CommandBuffer&>(commandBuffer).Get();

		if (m_timestampSupported)
		{
			vkCommandBuffer.resetQueryPool(m_timestampQueryPool.get(), m_renderPassIndex * 2, 2);
			vkCommandBuffer.writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe, m_timestampQueryPool.get(), m_renderPassIndex * 2);
		}

		if (m_statisticsSupported)
		{
			vkCommandBuffer.resetQueryPool(m_statisticsQueryPool.get(), m_renderPassIndex, 1);
			vkCommandBuffer.beginQuery(m_statisticsQueryPool.get(), m_renderPassIndex, vk::QueryControlFlags());
		}

		m_renderPassNames.emplace_back(passName);
	}

	void VulkanRenderStats::End(const ICommandBuffer& commandBuffer)
	{
		const vk::CommandBuffer& vkCommandBuffer = static_cast<const CommandBuffer&>(commandBuffer).Get();

		if (m_timestampSupported)
			vkCommandBuffer.writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, m_timestampQueryPool.get(), m_renderPassIndex * 2 + 1);

		if (m_statisticsSupported)
			vkCommandBuffer.endQuery(m_statisticsQueryPool.get(), m_renderPassIndex);

		++m_renderPassIndex;
	}

	void VulkanRenderStats::FinaliseResults(const IPhysicalDevice& physicalDevice, const IDevice& device, 
		const std::vector<IRenderResource*>& renderResources)
	{
		const Device& vkDevice = static_cast<const Device&>(device);
		const PhysicalDevice& vkPhysicalDevice = static_cast<const PhysicalDevice&>(physicalDevice);

		m_memoryStats.DedicatedBudget = 0;
		m_memoryStats.DedicatedUsage = 0;
		m_memoryStats.SharedBudget = 0;
		m_memoryStats.SharedUsage = 0;
		m_memoryStats.ResourceMemoryUsage.clear();

		for (const auto& renderResource : renderResources)
		{
			const char* name = renderResource->GetName();
			size_t size = renderResource->GetMemoryUsage();
			m_memoryStats.ResourceMemoryUsage.emplace(name, size);
		}

		vk::PhysicalDeviceMemoryBudgetPropertiesEXT budgetProperties{};
		vk::PhysicalDeviceMemoryProperties2 memoryProperties{};
		memoryProperties.pNext = &budgetProperties;
		vkPhysicalDevice.Get().getMemoryProperties2(&memoryProperties);
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
		const vk::Device& deviceImp = vkDevice.Get();

		for (uint32_t i = 0; i < m_renderPassCount; ++i)
		{
			FrameStats& data = m_statsData[m_renderPassNames[i]];
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

		m_renderPassNames.clear();

		// Add a 'Total' entry.
		FrameStats total = {};
		for (const auto& entry : m_statsData)
		{
			const FrameStats& data = entry.second;
			total.InputAssemblyVertexCount += data.InputAssemblyVertexCount;
			total.InputAssemblyPrimitivesCount += data.InputAssemblyPrimitivesCount;
			total.VertexShaderInvocations += data.VertexShaderInvocations;
			total.FragmentShaderInvocations += data.FragmentShaderInvocations;
			total.RenderTime += data.RenderTime;
		}

		m_statsData["Total"] = total;
	}
}