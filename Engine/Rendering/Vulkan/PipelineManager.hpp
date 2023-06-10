#pragma once

#include <vulkan/vulkan.hpp>
#include <vector>
#include <memory>
#include <unordered_map>
#include "../Material.hpp"
#include "Core/Logging/Logger.hpp"

namespace Engine::Rendering::Vulkan
{
	class PhysicalDevice;
	class Device;
	class PipelineLayout;

	class PipelineManager
	{
	public:
		PipelineManager();
		bool Initialise(const PhysicalDevice& physicalDevice, const Device& device, uint32_t concurrentFrames,
			vk::Format swapchainFormat, vk::Format depthFormat);
		bool Update(const PhysicalDevice& physicalDevice, const Device& device, vk::Format swapchainFormat,
			vk::Format depthFormat) const;

		inline bool TryGetPipelineLayout(const std::string& name, PipelineLayout** pipelineLayout) const
		{
			const auto& find = m_pipelineLayouts.find(name);
			if (find != m_pipelineLayouts.end())
			{
				*pipelineLayout = find->second.get();
				return true;
			}

			Engine::Logging::Logger::Error("Pipeline with name '{}' not found.", name);
			return false;
		}

	private:
		void ParseMaterials();
		bool BuildMaterials(const PhysicalDevice& physicalDevice, const Device& device, uint32_t concurrentFrames,
			vk::Format swapchainFormat, vk::Format depthFormat);

		std::unordered_map<std::string, std::unique_ptr<PipelineLayout>> m_pipelineLayouts;
		std::vector<Material> m_materials;
	};
}