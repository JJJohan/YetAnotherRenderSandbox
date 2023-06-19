#pragma once

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

	class RenderStats
	{
	public:
		RenderStats();
		const std::vector<FrameStats>& GetFrameStats() const;
		const MemoryStats& GetMemoryStats() const;

	protected:
		std::vector<FrameStats> m_statsData;
		MemoryStats m_memoryStats;
	};
}