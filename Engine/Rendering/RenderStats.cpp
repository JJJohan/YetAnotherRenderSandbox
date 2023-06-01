#include "RenderStats.hpp"

namespace Engine::Rendering
{
	RenderStats::RenderStats()
		: m_statsData()
		, m_memoryStats()
	{
	}

	const std::vector<FrameStats>& RenderStats::GetFrameStats() const
	{
		return m_statsData;
	}

	const MemoryStats& RenderStats::GetMemoryStats() const
	{
		return m_memoryStats;
	}
}