#pragma once

#include <array>
#include <glm/glm.hpp>
#include <memory>

namespace Engine::Rendering
{
	class GBuffer;
	class IPhysicalDevice;
	class IDevice;
	class IImageSampler;
	class IImageView;
	class IRenderImage;
	class IResourceFactory;
	class ICommandBuffer;
	class IMaterialManager;
	class Material;

	class PostProcessing
	{
	public:
		PostProcessing(GBuffer& gBuffer);

		bool Initialise(const IPhysicalDevice& physicalDevice, const IDevice& device,
			const IMaterialManager& materialManager, const IResourceFactory& resourceFactory, const glm::uvec2& size);

		bool Rebuild(const IPhysicalDevice& physicalDevice, const IDevice& device, 
			const IResourceFactory& resourceFactory, const glm::uvec2& size);

		void TransitionTAAImageLayouts(const IDevice& device, const ICommandBuffer& commandBuffer) const;
		void BlitTAA(const IDevice& device, const ICommandBuffer& commandBuffer) const;

		void Draw(const ICommandBuffer& commandBuffer, uint32_t frameIndex) const;

		inline const glm::vec2& GetTAAJitter(const glm::vec2& size)
		{
			return m_taaJitterOffsets[++m_taaFrameIndex % m_taaJitterOffsets.size()];
		}

		inline const IImageView& GetTAAPrevImageView() const { return *m_taaPreviousImageViews[1]; };

		inline void SetEnabled(bool enabled) { m_enabled = enabled; };
		inline bool IsEnabled() { return m_enabled; };

	private:
		bool RebuildTAA(const IPhysicalDevice& physicalDevice, const IDevice& device, const IResourceFactory& resourceFactory,
			const IImageView& inputImageView, const glm::uvec2& size);

		bool CreateTAAImage(const IDevice& device, const IResourceFactory& resourceFactory, const glm::uvec2& size);

		bool m_enabled;
		std::unique_ptr<IImageSampler> m_linearSampler;
		std::unique_ptr<IImageSampler> m_nearestSampler;
		GBuffer& m_gBuffer;

		uint32_t m_taaFrameIndex;
		std::array<glm::vec2, 6> m_taaJitterOffsets;
		std::array<std::unique_ptr<IRenderImage>, 2> m_taaPreviousImages;
		std::array<std::unique_ptr<IImageView>, 2> m_taaPreviousImageViews;
		Material* m_taaMaterial;
	};
}