#pragma once

#include "Core/Macros.hpp"
#include <stdint.h>
#include <vector>

namespace Engine::Rendering
{
	struct FrameStats
	{
		uint64_t InputAssemblyVertexCount;
		uint64_t InputAssemblyPrimitivesCount;
		uint64_t VertexShaderInvocations;
		uint64_t FragmentShaderInvocations;
		float RenderTime;
	};

	struct MemoryStats
	{
		uint64_t GBuffer;
		uint64_t ShadowMap;
		uint64_t DedicatedUsage;
		uint64_t DedicatedBudget;
		uint64_t SharedUsage;
		uint64_t SharedBudget;
	};

	class IPhysicalDevice;
	class IDevice;
	class ICommandBuffer;

	class RenderStats
	{
	public:
		RenderStats();
		virtual ~RenderStats() = default;

		virtual bool Initialise(const IPhysicalDevice& physicalDevice, const IDevice& device, uint32_t renderPassCount) = 0;
		virtual void Begin(const ICommandBuffer& commandBuffer) = 0;
		virtual void End(const ICommandBuffer& commandBuffer) = 0;
		virtual void FinaliseResults(const IPhysicalDevice& physicalDevice, const IDevice& device) = 0;

		EXPORT const std::vector<FrameStats>& GetFrameStats() const;
		EXPORT const MemoryStats& GetMemoryStats() const;

	protected:
		std::vector<FrameStats> m_statsData;
		MemoryStats m_memoryStats;
	};
}