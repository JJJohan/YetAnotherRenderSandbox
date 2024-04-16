#pragma once

#include <optional>

namespace Engine::Rendering
{
	struct QueueFamilyIndices
	{
		std::optional<uint32_t> GraphicsFamily;
		std::optional<uint32_t> PresentFamily;
		std::optional<uint32_t> ComputeFamily;
		std::optional<uint32_t> TransferFamily;

		bool IsComplete() const
		{
			return GraphicsFamily.has_value() && PresentFamily.has_value() && ComputeFamily.has_value() && TransferFamily.has_value();
		}
	};
}