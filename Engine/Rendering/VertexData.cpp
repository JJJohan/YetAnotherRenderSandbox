#include "VertexData.hpp"

namespace Engine::Rendering
{
	VertexData::VertexData()
		: m_data()
		, m_elementCount(0)
		, m_elementSize(0)
		, m_hash(0)
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

	uint64_t VertexData::GetHash() const
	{
		return m_hash;
	}

	void VertexData::ReplaceData(const std::vector<uint8_t>& data, uint32_t newCount)
	{
		m_data = data;
		m_elementCount = newCount;
		m_hash = Hash::CalculateHash(m_data);
	}
}