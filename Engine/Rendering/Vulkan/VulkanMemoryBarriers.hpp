#pragma once

#include "../Resources/IMemoryBarriers.hpp"
#include <vulkan/vulkan.hpp>
#include <vector>

namespace Engine::Rendering
{
	class VulkanMemoryBarriers : public IMemoryBarriers
	{
	public:
		VulkanMemoryBarriers()
			: m_memoryBarriers()
			, m_bufferMemoryBarriers()
			, m_imageMemoryBarriers()
		{
		}

		inline const std::vector<vk::MemoryBarrier2>& GetMemoryBarriers() const { return m_memoryBarriers; }
		inline const std::vector<vk::BufferMemoryBarrier2>& GetBufferMemoryBarriers() const { return m_bufferMemoryBarriers; }
		inline const std::vector<vk::ImageMemoryBarrier2>& GetImageMemoryBarriers() const { return m_imageMemoryBarriers; }

		inline void AddMemoryBarrier(const vk::MemoryBarrier2& memoryBarrier) { m_memoryBarriers.emplace_back(memoryBarrier); }
		inline void AddBufferMemoryBarrier(const vk::BufferMemoryBarrier2& memoryBarrier) { m_bufferMemoryBarriers.emplace_back(memoryBarrier); }
		inline void AddImageMemoryBarrier(const vk::ImageMemoryBarrier2& memoryBarrier) { m_imageMemoryBarriers.emplace_back(memoryBarrier); }

		inline virtual bool Empty() const override
		{
			return m_memoryBarriers.empty() && m_bufferMemoryBarriers.empty() && m_imageMemoryBarriers.empty();
		}

		inline virtual void Clear() override
		{
			m_memoryBarriers.clear();
			m_bufferMemoryBarriers.clear();
			m_imageMemoryBarriers.clear();
		}

	private:
		std::vector<vk::MemoryBarrier2> m_memoryBarriers;
		std::vector<vk::BufferMemoryBarrier2> m_bufferMemoryBarriers;
		std::vector<vk::ImageMemoryBarrier2> m_imageMemoryBarriers;
	};
}