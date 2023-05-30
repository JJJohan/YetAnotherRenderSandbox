#pragma once

#include <cstdint>
#include <vector>

namespace Engine::Rendering
{
	struct RenderStatsData
	{
		uint64_t InputAssemblyVertexCount;
		uint64_t InputAssemblyPrimitivesCount;
		uint64_t VertexShaderInvocations;
		uint64_t FragmentShaderInvocations;
		float RenderTime;
	};

	class RenderStats
	{
	public:
		RenderStats();
		const std::vector<RenderStatsData>& GetStatistics() const;

	protected:
		std::vector<RenderStatsData> m_statsData;
	};
}