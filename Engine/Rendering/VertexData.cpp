#include "VertexData.hpp"

namespace Engine::Rendering
{
	VertexData::VertexData()
		: m_data()
		, m_elementCount(0)
		, m_elementSize(0)
	{
	}

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