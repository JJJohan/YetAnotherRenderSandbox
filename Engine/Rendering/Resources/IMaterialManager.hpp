#pragma once

#include <memory>
#include <unordered_map>
#include "Core/Logging/Logger.hpp"
#include "../Types.hpp"

namespace Engine::Rendering
{
	class IPhysicalDevice;
	class IDevice;
	class Material;

	class IMaterialManager
	{
	public:
		IMaterialManager()
			: m_materials()
		{
		}

		virtual ~IMaterialManager() = default;
		virtual bool Initialise(const IPhysicalDevice& physicalDevice, const IDevice& device, uint32_t concurrentFrames,
			Format swapchainFormat, Format depthFormat) = 0;
		virtual bool Update(const IPhysicalDevice& physicalDevice, const IDevice& device, Format swapchainFormat,
			Format depthFormat) const = 0;

		inline bool TryGetMaterial(const std::string& name, Material** material) const
		{
			const auto& find = m_materials.find(name);
			if (find != m_materials.end())
			{
				*material = find->second.get();
				return true;
			}

			Engine::Logging::Logger::Error("Material with name '{}' not found.", name);
			return false;
		}

	protected:
		virtual bool BuildMaterials(const IPhysicalDevice& physicalDevice, const IDevice& device, uint32_t concurrentFrames,
			Format swapchainFormat, Format depthFormat) = 0;

		std::unordered_map<std::string, std::unique_ptr<Material>> m_materials;
	};
}