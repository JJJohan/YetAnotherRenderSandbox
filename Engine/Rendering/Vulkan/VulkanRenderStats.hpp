#pragma once

#include <vulkan/vulkan.hpp>
#include "../RenderStats.hpp"

namespace Engine::Rendering
{
	class ICommandBuffer;
}

namespace Engine::Rendering::Vulkan
{
	class VulkanRenderStats : public RenderStats
	{
	public:
		VulkanRenderStats();
		virtual bool Initialise(const IPhysicalDevice& physicalDevice, const IDevice& device, uint32_t renderPassCount) override;
		virtual void Begin(const ICommandBuffer& commandBuffer, std::string_view passName, bool isCompute) override;
		virtual void End(const ICommandBuffer& commandBuffer, bool isCompute) override;
		virtual void FinaliseResults(const IPhysicalDevice& physicalDevice, const IDevice& device,
			const std::vector<IRenderResource*>& renderResources) override;

	private:
		vk::UniqueQueryPool m_statisticsQueryPool;
		vk::UniqueQueryPool m_timestampQueryPool;
		std::vector<std::string> m_renderPassNames;
		std::vector<bool> m_isComputePass;

		bool m_timestampSupported;
		bool m_statisticsSupported;
		float m_timestampPeriod;
		uint32_t m_renderPassCount;
		uint32_t m_renderPassIndex;
	};
}