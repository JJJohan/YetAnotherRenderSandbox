#pragma once

#include <stdint.h>
#include "../Types.hpp"
#include <glm/glm.hpp>
#include "IImageView.hpp"
#include <memory>

namespace Engine::Rendering
{
	class IDevice;
	class IPhysicalDevice;
	class ICommandBuffer;

	class IRenderImage
	{
	public:
		IRenderImage()
			: m_format(Format::Undefined)
			, m_mipLevels(1)
			, m_layerCount(1)
			, m_dimensions()
			, m_layout(ImageLayout::Undefined)
			, m_imageView(nullptr)
			, m_usageFlags()
		{
		}

		virtual ~IRenderImage() = default;

		virtual bool Initialise(const IDevice& device, ImageType imageType, Format format, const glm::uvec3& dimensions,
			uint32_t mipLevels, uint32_t layerCount, ImageTiling tiling, ImageUsageFlags imageUsage, ImageAspectFlags aspectFlags,
			MemoryUsage memoryUsage, AllocationCreateFlags createFlags, SharingMode sharingMode) = 0;
		virtual bool InitialiseView(const IDevice& device, ImageAspectFlags aspectFlags) = 0;

		virtual bool UpdateContents(const void* data, uint64_t size) = 0;
		virtual void TransitionImageLayout(const IDevice& device, const ICommandBuffer& commandBuffer, ImageLayout newLayout) = 0;
		virtual void TransitionImageLayoutExt(const IDevice& device, const ICommandBuffer& commandBuffer,
			MaterialStageFlags newStageFlags, ImageLayout newLayout, MaterialAccessFlags newAccessFlags) = 0;
		virtual void TransitionImageLayoutExt(const IDevice& device, const ICommandBuffer& commandBuffer, 
			MaterialStageFlags oldStageFlags, ImageLayout oldLayout, MaterialAccessFlags oldAccessFlags,
			MaterialStageFlags newStageFlags, ImageLayout newLayout, MaterialAccessFlags newAccessFlags) = 0;
		virtual void GenerateMipmaps(const IDevice& device, const ICommandBuffer& commandBuffer) = 0;
		virtual bool CreateView(const IDevice& device, uint32_t baseMipLevel, ImageAspectFlags aspectFlags, std::unique_ptr<IImageView>& imageView) const = 0;

		inline const glm::uvec3& GetDimensions() const { return m_dimensions; }
		inline Format GetFormat() const { return m_format; }
		inline ImageUsageFlags GetUsageFlags() const { return m_usageFlags; }
		inline uint32_t GetMiplevels() const { return m_mipLevels; }
		inline uint32_t GetLayerCount() const { return m_layerCount; }
		inline ImageLayout GetLayout() const { return m_layout; }
		inline const IImageView& GetView() const { return *m_imageView; }

	protected:
		Format m_format;
		uint32_t m_mipLevels;
		uint32_t m_layerCount;
		ImageUsageFlags m_usageFlags;
		glm::uvec3 m_dimensions;
		ImageLayout m_layout;
		std::unique_ptr<IImageView> m_imageView;
	};
}