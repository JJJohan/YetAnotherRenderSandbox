#pragma once

#include "Core/Macros.hpp"
#include <vector>

namespace Engine::Rendering
{
	class VertexData
	{
	public:
		VertexData();

		template <typename T>
		VertexData(std::initializer_list<T> data)
			: m_elementSize(static_cast<uint32_t>(sizeof(std::decay_t<T>)))
			, m_elementCount(static_cast<uint32_t>(data.size()))
		{
			m_data.resize(m_elementSize * data.size());
			memcpy(m_data.data(), data.begin(), m_elementSize * data.size()); // Is this avoidable without overly complex templating?
		}

		template <typename T>
		VertexData(const std::vector<T>& data)
			: m_elementSize(static_cast<uint32_t>(sizeof(std::decay_t<T>)))
			, m_elementCount(static_cast<uint32_t>(data.size()))
		{
			m_data.resize(m_elementSize * data.size());
			memcpy(m_data.data(), data.data(), m_elementSize * data.size());
		}

		const void* GetData() const;
		uint32_t GetCount() const;
		uint32_t GetElementSize() const;

	private:
		uint32_t m_elementCount;
		uint32_t m_elementSize;
		std::vector<uint8_t> m_data;
	};
}