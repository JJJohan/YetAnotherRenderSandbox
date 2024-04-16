#include "DescriptorPool.hpp"
#include "Core/Logger.hpp"
#include "Device.hpp"

namespace Engine::Rendering::Vulkan
{
	DescriptorPool::DescriptorPool()
		: m_descriptorPool(nullptr)
	{
	}

	bool DescriptorPool::Initialise(const Device& device, uint32_t maxSets, const std::vector<vk::DescriptorPoolSize>& poolSizes, vk::DescriptorPoolCreateFlagBits flags)
	{
		vk::DescriptorPoolCreateInfo poolInfo(flags, maxSets, poolSizes);
		m_descriptorPool = device.Get().createDescriptorPoolUnique(poolInfo);
		if (!m_descriptorPool.get())
		{
			Logger::Error("Failed to create descriptor pool.");
			return false;
		}

		return true;
	}

	std::vector<vk::DescriptorSet> DescriptorPool::CreateDescriptorSets(const Device& device, const std::vector<vk::DescriptorSetLayout>& layouts)
	{
		vk::DescriptorSetAllocateInfo allocInfo(m_descriptorPool.get(), layouts);
		return device.Get().allocateDescriptorSets(allocInfo);
	}
}