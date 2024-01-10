#pragma once

#include <vulkan/vulkan.hpp>
#include "../Resources/ICommandPool.hpp"

namespace Engine::Rendering::Vulkan
{
	class CommandPool : public ICommandPool
	{
	public:
		CommandPool();
		~CommandPool();

		inline const vk::CommandPool& Get() const { return m_commandPool.get(); }
		virtual bool Initialise(const char* name, const IPhysicalDevice& physicalDevice, const IDevice& device, uint32_t queueFamilyIndex, CommandPoolFlags flags) override;

		virtual std::vector<std::unique_ptr<ICommandBuffer>> CreateCommandBuffers(const char* name, const IDevice& device, uint32_t count) const override;
		virtual std::unique_ptr<ICommandBuffer> BeginResourceCommandBuffer(const IDevice& device) const override;
		virtual void Reset(const IDevice& device) const override;

	private:
		vk::UniqueCommandPool m_commandPool;
	};
}