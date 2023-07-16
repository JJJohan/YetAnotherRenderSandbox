#pragma once

#include <vulkan/vulkan.hpp>
#include "../QueueFamilyIndices.hpp"
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
		bool Initialise(const Instance& instance, const Surface& surface);
		inline const vk::PhysicalDevice& Get() const { return m_physicalDevice; }
		std::vector<const char*> GetRequiredExtensions() const;
		bool FindMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties, uint32_t* memoryType) const;
		inline const vk::PhysicalDeviceLimits& GetLimits() const { return m_deviceProperties.limits; }
		inline const vk::PhysicalDeviceFeatures& GetFeatures() const { return m_deviceFeatures; }
		vk::SampleCountFlagBits GetMaxMultiSampleCount() const;
		Format FindSupportedFormat(const std::vector<Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features) const;
		virtual Format FindDepthFormat() const override;
		virtual bool FormatSupported(Format format) const override;
		inline virtual float GetMaxAnisotropy() const override { return m_deviceProperties.limits.maxSamplerAnisotropy; }
		inline virtual bool SupportsBCTextureCompression() const override { return m_deviceFeatures.textureCompressionBC; }
		inline virtual const QueueFamilyIndices& GetQueueFamilyIndices() const override { return m_queueFamilyIndices; }

	private:
		vk::PhysicalDevice m_physicalDevice;
		QueueFamilyIndices m_queueFamilyIndices;
		vk::PhysicalDeviceProperties m_deviceProperties;
		vk::PhysicalDeviceFeatures m_deviceFeatures;
	};
}