#pragma once

#include "Core/Macros.hpp"
#include "Core/Hash.hpp"
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
			m_hash = Hash::CalculateHash(m_data);
		}

		template <typename T>
		VertexData(const std::vector<T>& data)
			: m_elementSize(static_cast<uint32_t>(sizeof(std::decay_t<T>)))
			, m_elementCount(static_cast<uint32_t>(data.size()))
		{
			m_data.resize(m_elementSize * data.size());
			memcpy(m_data.data(), data.data(), m_elementSize * data.size());
			m_hash = Hash::CalculateHash(m_data);
		}

		void ReplaceData(const std::vector<uint8_t>& data, uint32_t newCount);

		inline const void* GetData() const { return m_data.data(); }
		inline uint32_t GetCount() const { return m_elementCount; }
		inline uint32_t GetElementSize() const { return m_elementSize; }
		inline uint64_t GetHash() const { return m_hash; }

	private:
		uint64_t m_hash;
		uint32_t m_elementCount;
		uint32_t m_elementSize;
		std::vector<uint8_t> m_data;
	};
}