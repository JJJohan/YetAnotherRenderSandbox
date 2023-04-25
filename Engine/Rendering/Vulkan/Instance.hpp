#pragma once

#include <vulkan/vulkan.hpp>

namespace Engine::Rendering::Vulkan
{
	class Debug;

	class Instance
	{
	public:
		Instance();
		bool Initialise(std::string name, Debug& debug, bool useDebug);
		const vk::Instance& Get() const;

	private:
		vk::UniqueInstance m_instance;
	};
}