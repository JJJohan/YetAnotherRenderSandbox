#pragma once

#include <vulkan/vulkan.hpp>
#include "../IMaterialManager.hpp"

namespace Engine::Rendering::Vulkan
{
	class PipelineManager : public IMaterialManager
	{
	public:
		PipelineManager();
		bool Initialise(const IPhysicalDevice& physicalDevice, const IDevice& device, uint32_t concurrentFrames,
			Format swapchainFormat, Format depthFormat) override;
		bool Update(const IPhysicalDevice& physicalDevice, const IDevice& device, Format swapchainFormat,
			Format depthFormat) const override;

		bool CheckDirty() const;
		void WritePipelineCache(const IDevice& device) const;
		inline const VkPipelineCache GetPipelineCache() const { return m_pipelineCache.get(); }

	private:
		virtual bool BuildMaterials(const IPhysicalDevice& physicalDevice, const IDevice& device, uint32_t concurrentFrames,
			Format swapchainFormat, Format depthFormat) override;

		vk::UniquePipelineCache m_pipelineCache;
	};
}