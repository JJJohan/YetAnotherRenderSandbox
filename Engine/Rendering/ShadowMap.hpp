#pragma once

#include <glm/glm.hpp>
#include "Types.hpp"
#include <vector>
#include <memory>
#include "Resources/AttachmentInfo.hpp"
#include "Resources/IRenderImage.hpp"

namespace Engine::Rendering
{
	class IDevice;
	class IBuffer;
	class IResourceFactory;
	class Camera;

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
		bool Rebuild(const IDevice& device, const IResourceFactory& resourceFactory, Format depthFormat);
		ShadowCascadeData UpdateCascades(const Camera& camera, const glm::vec3& lightDir);

		inline IRenderImage& GetShadowImage(uint32_t index) const { return *m_shadowImages[index]; }
		inline std::vector<const IImageView*> GetShadowImageViews() const 
		{

			std::vector<const IImageView*> imageViews(m_shadowImages.size());
			for (size_t i = 0; i < m_shadowImages.size(); ++i)
				imageViews[i] = &m_shadowImages[i]->GetView();
			return imageViews;
		}

		inline ShadowCascadeData GetShadowCascadeData() const { return ShadowCascadeData(m_cascadeSplits, m_cascadeMatrices); }
		inline uint32_t GetCascadeCount() const { return m_cascadeCount; }

		inline AttachmentInfo GetShadowAttachment(uint32_t index) const
		{
			AttachmentInfo attachment{};
			attachment.imageView = &m_shadowImages[index]->GetView();
			attachment.imageLayout = ImageLayout::DepthAttachment;
			attachment.loadOp = AttachmentLoadOp::Clear;
			attachment.storeOp = AttachmentStoreOp::Store;
			attachment.clearValue = ClearValue(1.0f);
			return attachment;
		}

		inline uint64_t GetMemoryUsage() const
		{
			const uint32_t bytesPerTexel = 4;
			return m_cascadeCount * m_extent.x * m_extent.y * bytesPerTexel;
		}

	private:
		bool CreateShadowImages(const IDevice& device, const IResourceFactory& resourceFactory, Format depthFormat);

		glm::uvec3 m_extent;
		uint32_t m_cascadeCount;
		std::vector<float> m_cascadeSplits;
		std::vector<glm::mat4> m_cascadeMatrices;
		std::vector<std::unique_ptr<IRenderImage>> m_shadowImages;
	};
}