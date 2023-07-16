#pragma once

#include <vector>
#include "../Types.hpp"

namespace Engine::Rendering
{
	class ISemaphore;
	class ICommandBuffer;

	struct SubmitInfo
	{
		std::vector<uint64_t> WaitValues;
		std::vector<uint64_t> SignalValues;
		std::vector<MaterialStageFlags> Stages;
		std::vector<const ICommandBuffer*> CommandBuffers;
		std::vector<const ISemaphore*> WaitSemaphores;
		std::vector<const ISemaphore*> SignalSemaphores;

		SubmitInfo()
			: WaitSemaphores()
			, SignalSemaphores()
			, CommandBuffers()
			, WaitValues()
			, SignalValues()
			, Stages()
		{
		}
	};
}