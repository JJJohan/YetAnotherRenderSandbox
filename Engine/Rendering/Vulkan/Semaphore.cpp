#include "Semaphore.hpp"
#include "Core/Logging/Logger.hpp"
#include "Device.hpp"

using namespace Engine::Logging;

namespace Engine::Rendering::Vulkan
{
	Semaphore::Semaphore()
		: m_semaphore(nullptr)
	{
	}

	bool Semaphore::Initialise(std::string_view name, const IDevice& device, bool binary)
	{
		const Device& vkDevice = static_cast<const Device&>(device);

		vk::SemaphoreTypeCreateInfo typeCreateInfo(binary ? vk::SemaphoreType::eBinary : vk::SemaphoreType::eTimeline);
		vk::SemaphoreCreateInfo createInfo({}, &typeCreateInfo);

		m_semaphore = vkDevice.Get().createSemaphoreUnique(createInfo);
		if (!m_semaphore.get())
		{
			Logger::Error("Failed to create semaphore.");
			return false;
		}

		vkDevice.SetResourceName(ResourceType::Semaphore, m_semaphore.get(), name);

		return true;
	}
}