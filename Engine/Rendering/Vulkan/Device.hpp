#pragma once

#include <vulkan/vulkan.hpp>
#include "../Resources/IDevice.hpp"

namespace Engine::Rendering
{
	class IPhysicalDevice;
}

namespace Engine::Rendering::Vulkan
{
	class Device : public IDevice
	{
	public:
		Device();
		inline const vk::Device& Get() const { return m_device.get(); }
		inline const vk::Queue& GetGraphicsQueue() const { return m_graphicsQueue; }
		inline const vk::Queue& GetPresentQueue() const { return m_presentQueue; }
		inline const vk::Queue& GetComputeQueue() const { return m_computeQueue; }
		inline bool AsyncCompute() const { return m_graphicsQueue != m_computeQueue; }
		bool Initialise(const IPhysicalDevice& physicalDevice);

	private:
		vk::UniqueDevice m_device;
		vk::Queue m_graphicsQueue;
		vk::Queue m_computeQueue;
		vk::Queue m_presentQueue;
	};
}