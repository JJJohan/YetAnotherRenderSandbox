#pragma once

#include <cstdint>
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
		uint64_t TotalUsage;
		uint64_t TotalBudget;
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