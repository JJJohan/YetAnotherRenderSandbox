#pragma once

#include <glm/glm.hpp>
#include "../Types.hpp"
#include <memory>
#include <vector>
#include "../Resources/IRenderImage.hpp"
#include "IRenderResource.hpp"

#define GBUFFER_SIZE 5 // albedo, normal, worldPos, metalRoughness, velocity

namespace Engine::Rendering
{
	class IPhysicalDevice;
	class IDevice;
	class IResourceFactory;
	struct AttachmentInfo;

	class GBuffer : public IRenderResource
	{
	public:
		GBuffer();

		virtual bool Build(const Renderer& renderer) override;

		void TransitionImageLayouts(const IDevice& device, const ICommandBuffer& commandBuffer, ImageLayout newLayout);
		void TransitionDepthLayout(const IDevice& device, const ICommandBuffer& commandBuffer, ImageLayout newLayout);
		uint64_t GetMemoryUsage() const;

		inline const std::vector<std::unique_ptr<IRenderImage>>& GetImages() const { return m_gBufferImages; }
		inline std::vector<const IImageView*> GetImageViews() const
		{
			std::vector<const IImageView*> views(m_gBufferImages.size());
			for (size_t i = 0; i < m_gBufferImages.size(); i++)
			{
				views[i] = &m_gBufferImages[i]->GetView();
			}

			return views;
		}

		std::vector<AttachmentInfo> GetRenderAttachments() const;
		AttachmentInfo GetDepthAttachment() const;
		inline const IImageView& GetVelocityImageView() const { return m_gBufferImages[4]->GetView(); }
		inline const std::vector<Format>& GetImageFormats() const { return m_imageFormats; }
		inline const IImageView& GetDepthImageView() const { return m_depthImage->GetView(); }
		inline IRenderImage& GetDepthImage() const { return *m_depthImage; }

	private:
		bool CreateImageAndView(const IDevice& device, const IResourceFactory& resourceFactory, const glm::uvec2& size, Format format);
		bool CreateColorImages(const IDevice& device, const IResourceFactory& resourceFactory, const glm::uvec2& size);
		bool CreateDepthImage(const IDevice& device, const IResourceFactory& resourceFactory, const glm::uvec2& size, Format format);

		std::vector<std::unique_ptr<IRenderImage>> m_gBufferImages;
		std::vector<Format> m_imageFormats;
		std::unique_ptr<IRenderImage> m_depthImage;
	};
}