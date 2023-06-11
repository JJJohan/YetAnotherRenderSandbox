#pragma once

#include <vulkan/vulkan.hpp>

namespace Engine::Rendering::Vulkan
{
	class Device;

	class DescriptorPool
	{
	public:
		DescriptorPool();
		inline const vk::DescriptorPool& Get() const { return m_descriptorPool.get(); }
		bool Initialise(const Device& device, uint32_t maxSets, const std::vector<vk::DescriptorPoolSize>& poolSizes, vk::DescriptorPoolCreateFlagBits flags = {});

		std::vector<vk::DescriptorSet> CreateDescriptorSets(const Device& device, const std::vector<vk::DescriptorSetLayout>& layouts);

	private:
		vk::UniqueDescriptorPool m_descriptorPool;
	};
}