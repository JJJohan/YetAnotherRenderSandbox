#include "PipelineManager.hpp"
#include "Device.hpp"
#include "DescriptorPool.hpp"
#include "PipelineLayout.hpp"
#include "OS/Files.hpp"
#include "../IPhysicalDevice.hpp"
#include "../Resources/Material.hpp"
#include <filesystem>

using namespace Engine::Logging;
using namespace Engine::OS;

namespace Engine::Rendering::Vulkan
{
	PipelineManager::PipelineManager()
		: m_pipelineCache()
	{
	}

	bool PipelineManager::BuildMaterials(const IPhysicalDevice& physicalDevice, const IDevice& device, uint32_t concurrentFrames, Format swapchainFormat, Format depthFormat)
	{
		for (const auto& materialPath : std::filesystem::recursive_directory_iterator("materials"))
		{
			if (materialPath.is_regular_file())
			{
				const auto& path = materialPath.path();
				if (path.extension() == ".material")
				{
					std::unique_ptr<PipelineLayout> material = std::make_unique<PipelineLayout>();
					if (!material->Parse(path))
						continue;

					if (!material->Initialise(device, concurrentFrames))
					{
						return false;
					}

					m_materials.emplace(material->GetName(), std::move(material));
				}
			}
		}

		return true;
	}

	bool PipelineManager::Initialise(const IPhysicalDevice& physicalDevice, const IDevice& device, uint32_t concurrentFrames, Format swapchainFormat, Format depthFormat)
	{
		if (!BuildMaterials(physicalDevice, device, concurrentFrames, swapchainFormat, depthFormat))
		{
			return false;
		}

		std::vector<uint8_t> cacheData;
		Files::TryReadBinaryFile("pipelines.cache", cacheData);

		vk::PipelineCacheCreateInfo createInfo(vk::PipelineCacheCreateFlags(), cacheData.size(), cacheData.data());
		m_pipelineCache = static_cast<const Device&>(device).Get().createPipelineCacheUnique(createInfo);

		return true;
	}

	bool PipelineManager::CheckDirty() const
	{
		for (const auto& material : m_materials)
		{
			PipelineLayout* pipeline = static_cast<PipelineLayout*>(material.second.get());
			if (pipeline->IsDirty())
			{
				return true;
			}
		}

		return false;
	}

	bool PipelineManager::Update(const IPhysicalDevice& physicalDevice, const IDevice& device,
		Format swapchainFormat, Format depthFormat) const
	{
		for (const auto& material : m_materials)
		{
			PipelineLayout* pipeline = static_cast<PipelineLayout*>(material.second.get());
			if (!pipeline->Update(physicalDevice, device, m_pipelineCache.get(), swapchainFormat, depthFormat))
			{
				return false;
			}
		}

		return true;
	}

	void PipelineManager::WritePipelineCache(const IDevice& device) const
	{
		std::vector<uint8_t> cacheData = static_cast<const Device&>(device).Get().getPipelineCacheData(m_pipelineCache.get());

		if (!Files::TryWriteBinaryFile("materials.cache", cacheData))
		{
			Logger::Error("Failed to write m_materials cache to disk.");
		}
	}
}