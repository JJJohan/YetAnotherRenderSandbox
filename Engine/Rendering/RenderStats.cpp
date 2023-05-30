#include "RenderStats.hpp"

namespace Engine::Rendering
{
	RenderStats::RenderStats()
		: m_statsData()
	{
	}

	const std::vector<RenderStatsData>& RenderStats::GetStatistics() const
	{
		return m_statsData;
	}
}