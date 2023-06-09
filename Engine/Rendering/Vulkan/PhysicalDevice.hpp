#pragma once

#include <vulkan/vulkan.hpp>
#include "QueueFamilyIndices.hpp"
#include "../IPhysicalDevice.hpp"
#include "../Types.hpp"

namespace Engine::Rendering::Vulkan
{
	class Instance;
	class Surface;

	class PhysicalDevice : public IPhysicalDevice
	{
	public:
		PhysicalDevice();
		const QueueFamilyIndices& GetQueueFamilyIndices() const;
		bool Initialise(const Instance& instance, const Surface& surface);
		const vk::PhysicalDevice& Get() const;
		std::vector<const char*> GetRequiredExtensions() const;
		bool FindMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties, uint32_t* memoryType) const;
		const vk::PhysicalDeviceLimits& GetLimits() const;
		const vk::PhysicalDeviceFeatures& GetFeatures() const;
		vk::SampleCountFlagBits GetMaxMultiSampleCount() const;
		Format FindSupportedFormat(const std::vector<Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features) const;
		virtual Format FindDepthFormat() const override;
		virtual bool FormatSupported(Format format) const override;
		virtual float GetMaxAnisotropy() const override;
		virtual bool SupportsBCTextureCompression() const override;

	private:

		vk::PhysicalDevice m_physicalDevice;
		QueueFamilyIndices m_queueFamilyIndices;
		vk::PhysicalDeviceProperties m_deviceProperties;
		vk::PhysicalDeviceFeatures m_deviceFeatures;
	};
}