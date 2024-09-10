#include "ScrollingGraphBuffer.hpp"

namespace Engine::UI
{
	ScrollingGraphBuffer::ScrollingGraphBuffer(const std::string& label, uint32_t maxSize)
		: Label(label)
		, Capacity(maxSize)
	{
		Values.resize(maxSize);
	}

	void ScrollingGraphBuffer::SetValue(uint32_t offset, float value)
	{
		Values[offset % Capacity] = value;
	}

	void ScrollingGraphBuffer::Clear()
	{
		std::fill(Values.begin(), Values.end(), 0.0f);
	}
}