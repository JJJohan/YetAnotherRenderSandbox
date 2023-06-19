#pragma once

#include <stdint.h>

namespace Engine::Rendering
{
	struct IndexedIndirectCommand
	{
		uint32_t IndexCount;
		uint32_t InstanceCount;
		uint32_t FirstIndex;
		uint32_t VertexOffset;
		uint32_t FirstInstance;

		IndexedIndirectCommand(uint32_t indexCount = {}, uint32_t instanceCount = {}, uint32_t firstIndex = {}, int32_t vertexOffset = {}, uint32_t firstInstance = {})
			: IndexCount(indexCount)
			, InstanceCount(instanceCount)
			, FirstIndex(firstIndex)
			, VertexOffset(vertexOffset)
			, FirstInstance(firstInstance)
		{
		}
	};
}