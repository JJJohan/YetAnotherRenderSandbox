#include "PipelineManager.hpp"
#include "Device.hpp"
#include "PhysicalDevice.hpp"
#include "PipelineLayout.hpp"
#include "DescriptorPool.hpp"
#include "OS/Files.hpp"
#include <filesystem>

using namespace Engine::Logging;
using namespace Engine::OS;

namespace Engine::Rendering::Vulkan
{
	PipelineManager::PipelineManager()
		: m_pipelineLayouts()
		, m_pipelineCache()
	{
	}

	void PipelineManager::ParseMaterials()
	{
		for (const auto& materialPath : std::filesystem::recursive_directory_iterator("materials"))
		{
			if (materialPath.is_regular_file())
			{
				const auto& path = materialPath.path();
				if (path.extension() == ".material")
				{
					Material material;
					if (!material.Parse(path))
						continue;

					m_materials.push_back(material);
				}
			}
		}
	}

	bool PipelineManager::BuildMaterials(const PhysicalDevice& physicalDevice, const Device& device, uint32_t concurrentFrames, vk::Format swapchainFormat, vk::Format depthFormat)
	{
		for (auto& material : m_materials)
		{
			std::unique_ptr<PipelineLayout> pipelineLayout = std::make_unique<PipelineLayout>(material);
			if (!pipelineLayout->Initialise(device, concurrentFrames))
			{
				return false;
			}

			m_pipelineLayouts.emplace(material.GetName(), std::move(pipelineLayout));
		}

		return true;
	}

	bool PipelineManager::Initialise(const PhysicalDevice& physicalDevice, const Device& device, uint32_t concurrentFrames, vk::Format swapchainFormat, vk::Format depthFormat)
	{
		ParseMaterials();

		if (!BuildMaterials(physicalDevice, device, concurrentFrames, swapchainFormat, depthFormat))
		{
			return false;
		}

		std::vector<uint8_t> cacheData;
		Files::TryReadBinaryFile("pipelines.cache", cacheData);

		vk::PipelineCacheCreateInfo createInfo(vk::PipelineCacheCreateFlags(), cacheData.size(), cacheData.data());
		m_pipelineCache = device.Get().createPipelineCacheUnique(createInfo);

		return true;
	}

	bool PipelineManager::CheckDirty() const
	{
		for (const auto& pipeline : m_pipelineLayouts)
		{
			if (pipeline.second->IsDirty())
			{
				return true;
			}
		}

		return false;
	}

	bool PipelineManager::Update(const PhysicalDevice& physicalDevice, const Device& device,
		vk::Format swapchainFormat, vk::Format depthFormat) const
	{
		for (const auto& pipeline : m_pipelineLayouts)
		{
			if (!pipeline.second->Update(physicalDevice, device, m_pipelineCache.get(), swapchainFormat, depthFormat))
			{
				return false;
			}
		}

		return true;
	}

	void PipelineManager::WritePipelineCache(const Device& device) const
	{
		std::vector<uint8_t> cacheData = device.Get().getPipelineCacheData(m_pipelineCache.get());

		if (!Files::TryWriteBinaryFile("pipelines.cache", cacheData))
		{
			Logger::Error("Failed to write pipeline cache to disk.");
		}
	}
}