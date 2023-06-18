#pragma once

#include <glm/glm.hpp>
#include "Types.hpp"
#include <memory>
#include <vector>

#define GBUFFER_SIZE 5 // albedo, normal, worldPos, metalRoughness, velocity

namespace Engine::Rendering
{
	class ShadowMap;
	class IRenderImage;
	class IImageView;
	class IImageSampler;
	class IPhysicalDevice;
	class IDevice;
	class ICommandBuffer;
	class IBuffer;
	class IResourceFactory;
	class IMaterialManager;
	struct AttachmentInfo;
	class Material;

	class GBuffer
	{
	public:
		GBuffer();

		bool Initialise(const IPhysicalDevice& physicalDevice, const IDevice& device,
			const IMaterialManager& materialManager, const IResourceFactory& resourceFactory,
			Format depthFormat, const glm::uvec2& size, const std::vector<std::unique_ptr<IBuffer>>& frameInfoBuffers,
			const std::vector<std::unique_ptr<IBuffer>>& lightBuffers, const ShadowMap& shadowMap);
		bool Rebuild(const IPhysicalDevice& physicalDevice, const IDevice& device, const IResourceFactory& resourceFactory,
			const glm::uvec2& size, const std::vector<std::unique_ptr<IBuffer>>& frameInfoBuffers,
			const std::vector<std::unique_ptr<IBuffer>>& lightBuffers, const ShadowMap& shadowMap);
		void TransitionImageLayouts(const IDevice& device, const ICommandBuffer& commandBuffer, ImageLayout newLayout);
		void TransitionDepthLayout(const IDevice& device, const ICommandBuffer& commandBuffer, ImageLayout newLayout);
		void DrawFinalImage(const ICommandBuffer& commandBuffer, uint32_t currentFrameIndex) const;
		uint64_t GetMemoryUsage() const;
		void SetDebugMode(uint32_t value) const;

		std::vector<AttachmentInfo> GetRenderAttachments() const;
		AttachmentInfo GetDepthAttachment() const;
		inline const IImageView& GetVelocityImageView() const { return *m_gBufferImageViews[4]; }
		inline const std::vector<Format>& GetImageFormats() const { return m_imageFormats; }
		inline Format GetDepthFormat() const { return m_depthFormat; }
		inline IRenderImage& GetOutputImage() const { return *m_outputImage; }
		inline const IImageView& GetOutputImageView() const { return *m_outputImageView; }
		inline const IImageView& GetDepthImageView() const { return *m_depthImageView; }

	private:
		bool CreateImageAndView(const IDevice& device, const IResourceFactory& resourceFactory, const glm::uvec2& size, Format format);
		bool CreateColorImages(const IDevice& device, const IResourceFactory& resourceFactory, const glm::uvec2& size);
		bool CreateDepthImage(const IDevice& device, const IResourceFactory& resourceFactory, const glm::uvec2& size);
		bool CreateOutputImage(const IDevice& device, const IResourceFactory& resourceFactory, const glm::uvec2& size);

		std::vector<std::unique_ptr<IRenderImage>> m_gBufferImages;
		std::vector<std::unique_ptr<IImageView>> m_gBufferImageViews;
		std::vector<Format> m_imageFormats;
		std::unique_ptr<IRenderImage> m_depthImage;
		std::unique_ptr<IImageView> m_depthImageView;
		std::unique_ptr<IRenderImage> m_outputImage;
		std::unique_ptr<IImageView> m_outputImageView;
		std::unique_ptr<IImageSampler> m_sampler;
		std::unique_ptr<IImageSampler> m_shadowSampler;
		Format m_depthFormat;
		Material* m_combineMaterial;
	};
}