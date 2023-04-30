#include "VertexData.hpp"

namespace Engine::Rendering
{
	const void* VertexData::GetData() const
	{
		return m_data.data();
	}

	uint32_t VertexData::GetCount() const
	{
		return m_elementCount;
	}

	uint32_t VertexData::GetElementSize() const
	{
		return m_elementSize;
	}
}