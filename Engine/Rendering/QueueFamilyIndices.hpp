#pragma once

#include <optional>

namespace Engine::Rendering
{
	struct QueueFamilyIndices
	{
		std::optional<uint32_t> GraphicsFamily;
		std::optional<uint32_t> PresentFamily;
		std::optional<uint32_t> ComputeFamily;

		bool IsComplete()
		{
			return GraphicsFamily.has_value() && PresentFamily.has_value() && ComputeFamily.has_value();
		}
	};
}