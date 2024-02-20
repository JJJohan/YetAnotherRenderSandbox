#pragma once

#include <vulkan/vulkan.hpp>
#include "../Resources/ISemaphore.hpp"

namespace Engine::Rendering::Vulkan
{
	class Semaphore : public ISemaphore
	{
	public:
		Semaphore();

		virtual bool Initialise(std::string_view name, const IDevice& device, bool binary) override;

		inline const vk::Semaphore& Get() const { return m_semaphore.get(); }

	private:
		vk::UniqueSemaphore m_semaphore;
	};
}