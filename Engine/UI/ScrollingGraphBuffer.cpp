#include "ScrollingGraphBuffer.hpp"

namespace Engine::UI
{
	ScrollingGraphBuffer::ScrollingGraphBuffer(const std::string& label, uint32_t maxSize)
		: Label(label)
		, Capacity(maxSize)
		, m_offset(0)
	{
		Values.reserve(maxSize);
	}

	void ScrollingGraphBuffer::AddValue(float value)
	{
		if (Values.size() < Capacity)
		{
			Values.push_back(value);
		}
		else
		{
			Values[m_offset] = value;
			m_offset = (m_offset + 1) % Capacity;
		}
	}

	void ScrollingGraphBuffer::Clear()
	{
		if (!Values.empty())
		{
			Values.clear();
			m_offset = 0;
		}
	}
}