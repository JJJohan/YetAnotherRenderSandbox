#pragma once

#include <vulkan/vulkan.hpp>

namespace Engine::Rendering::Vulkan
{
	class Debug;

	class Instance
	{
	public:
		Instance();
		inline const vk::Instance& Get() const { return m_instance.get(); }
		bool Initialise(std::string name, Debug& debug, bool useDebug);

	private:
		vk::UniqueInstance m_instance;
	};
}