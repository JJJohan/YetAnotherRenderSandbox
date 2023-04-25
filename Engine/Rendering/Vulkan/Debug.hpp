#pragma once

#include <vector>
#include <vulkan/vulkan.hpp>

namespace Engine::Rendering::Vulkan
{
	class Instance;

	class Debug
	{
	public:
		Debug();
		void SetupDebugCallback(const Instance& instance);
		bool CheckValidationLayerSupport(const std::vector<const char*>& validationLayers) const;
		void PopulateDebugMessengerCreateInfo(vk::DebugUtilsMessengerCreateInfoEXT& createInfo) const;

	private:
		vk::UniqueDebugUtilsMessengerEXT m_debugMessenger;
	};
}