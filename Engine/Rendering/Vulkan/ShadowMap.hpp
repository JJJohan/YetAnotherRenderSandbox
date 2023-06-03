#pragma once

#include <vulkan/vulkan.hpp>
#include <vma/vk_mem_alloc.h>
#include <glm/glm.hpp>
#include "ImageView.hpp"
#include "RenderImage.hpp"

namespace Engine::Rendering
{
	class Camera;
}

namespace Engine::Rendering::Vulkan
{
	class Device;
	class ImageView;
	class RenderImage;
	class Buffer;

	struct ShadowCascadeData
	{
		const std::vector<float>& CascadeSplits;
		const std::vector<glm::mat4>& CascadeMatrices;

		ShadowCascadeData(const std::vector<float>& splits, const std::vector<glm::mat4>& matrices)
			: CascadeSplits(splits)
			, CascadeMatrices(matrices)
		{
		}
	};

	class ShadowMap
	{
	public:
		ShadowMap();
		bool Rebuild(const Device& device, VmaAllocator allocator, vk::Format depthFormat);
		ShadowCascadeData UpdateCascades(const Camera& camera, const glm::vec3& lightDir);

		inline RenderImage& GetShadowImage(uint32_t index) const { return *m_shadowImages[index]; }
		inline const ImageView& GetShadowImageView(uint32_t index) const { return *m_shadowImageViews[index]; }
		inline ShadowCascadeData GetShadowCascadeData() const { return ShadowCascadeData(m_cascadeSplits, m_cascadeMatrices); }
		inline uint32_t GetCascadeCount() const { return m_cascadeCount; }

		inline vk::RenderingAttachmentInfo GetShadowAttachment(uint32_t index) const
		{
			vk::RenderingAttachmentInfo attachment{};
			attachment.imageView = m_shadowImageViews[index]->Get();
			attachment.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
			attachment.loadOp = vk::AttachmentLoadOp::eClear;
			attachment.storeOp = vk::AttachmentStoreOp::eStore;
			attachment.clearValue = vk::ClearValue(vk::ClearDepthStencilValue(1.0f, 0));
			return attachment;
		}

		inline uint64_t GetMemoryUsage() const
		{
			const uint32_t bytesPerTexel = 4;
			return m_cascadeCount * m_extent.width * m_extent.height * bytesPerTexel;
		}

	private:
		bool CreateShadowImages(const Device& device, VmaAllocator allocator, vk::Format depthFormat);

		vk::Extent3D m_extent;
		uint32_t m_cascadeCount;
		std::vector<float> m_cascadeSplits;
		std::vector<glm::mat4> m_cascadeMatrices;
		std::vector<std::unique_ptr<RenderImage>> m_shadowImages;
		std::vector<std::unique_ptr<ImageView>> m_shadowImageViews;
	};
}