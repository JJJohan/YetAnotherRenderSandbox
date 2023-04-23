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
		bool SetupDebugCallback(const Instance& instance);
		bool RemoveDebugCallback(const Instance& instance) const;
		bool CheckValidationLayerSupport(const std::vector<const char*>& validationLayers) const;
		void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) const;

	private:
		VkDebugUtilsMessengerEXT m_debugMessenger;
	};
}