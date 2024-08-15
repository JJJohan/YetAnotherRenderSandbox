#pragma once

#include "../Resources/IImageMemoryBarriers.hpp"
#include <vulkan/vulkan.hpp>
#include <vector>

namespace Engine::Rendering
{
	class VulkanImageMemoryBarriers : public IImageMemoryBarriers
	{
	public:
		VulkanImageMemoryBarriers()
			: m_imageMemoryBarriers()
		{
		}

		inline const std::vector<vk::ImageMemoryBarrier2>& GetMemoryBarriers() const { return m_imageMemoryBarriers; }
		inline void AddMemoryBarrier(const vk::ImageMemoryBarrier2& memoryBarrier) { m_imageMemoryBarriers.emplace_back(memoryBarrier); }

		inline virtual bool Empty() const override { return m_imageMemoryBarriers.empty(); };
		inline virtual void Clear() override { m_imageMemoryBarriers.clear(); }

	private:
		std::vector<vk::ImageMemoryBarrier2> m_imageMemoryBarriers;
	};
}