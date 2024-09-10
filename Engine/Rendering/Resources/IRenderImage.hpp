#pragma once

#include <stdint.h>
#include "../Types.hpp"
#include <glm/glm.hpp>
#include "IImageView.hpp"
#include <memory>
#include <string_view>

namespace Engine::Rendering
{
	class IDevice;
	class IPhysicalDevice;
	class ICommandBuffer;
	class IMemoryBarriers;

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

		virtual bool Initialise(std::string_view name, const IDevice& device, ImageType imageType,
			Format format, const glm::uvec3& dimensions, uint32_t mipLevels, uint32_t layerCount,
			ImageTiling tiling, ImageUsageFlags imageUsage, ImageAspectFlags aspectFlags,
			MemoryUsage memoryUsage, AllocationCreateFlags createFlags, SharingMode sharingMode,
			bool preinitialise = false) = 0;

		virtual bool InitialiseView(std::string_view name, const IDevice& device, ImageAspectFlags aspectFlags) = 0;

		virtual bool UpdateContents(const void* data, size_t offset, uint64_t size) = 0;

		virtual bool AppendImageLayoutTransition(const ICommandBuffer& commandBuffer,
			ImageLayout newLayout, IMemoryBarriers& memoryBarriers, uint32_t srcQueueFamily = 0,
			uint32_t dstQueueFamily = 0, bool compute = false) = 0;

		virtual bool AppendImageLayoutTransitionExt(const ICommandBuffer& commandBuffer,
			MaterialStageFlags newStageFlags, ImageLayout newLayout, MaterialAccessFlags newAccessFlags,
			IMemoryBarriers& memoryBarriers, uint32_t baseMipLevel = 0, uint32_t mipLevelCount = 0,
			uint32_t srcQueueFamily = 0, uint32_t dstQueueFamily = 0, bool compute = false) = 0;

		virtual void GenerateMipmaps(const ICommandBuffer& commandBuffer) = 0;

		virtual bool CreateView(std::string_view name, const IDevice& device, uint32_t baseMipLevel,
			ImageAspectFlags aspectFlags, std::unique_ptr<IImageView>& imageView) const = 0;

		inline const glm::uvec3& GetDimensions() const { return m_dimensions; }
		inline Format GetFormat() const { return m_format; }
		inline ImageUsageFlags GetUsageFlags() const { return m_usageFlags; }
		inline uint32_t GetMipLevels() const { return m_mipLevels; }
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