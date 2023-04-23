#pragma once

#include <vulkan/vulkan.hpp>

namespace Engine::Rendering::Vulkan
{
	class Debug;

	class Instance
	{
	public:
		Instance();
		void Shutdown();
		bool CreateInstance(std::string name, Debug& debug, bool useDebug);
		VkInstance Get() const;

	private:
		VkInstance m_instance;
	};
}