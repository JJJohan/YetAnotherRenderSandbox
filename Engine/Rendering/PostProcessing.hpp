#pragma once

#include <array>
#include <glm/glm.hpp>
#include <memory>
#include "Resources/IRenderImage.hpp"
#include "Resources/IImageSampler.hpp"
#include "Resources/Material.hpp"

namespace Engine::Rendering
{
	class GBuffer;
	class IPhysicalDevice;
	class IDevice;
	class IImageSampler;
	class IResourceFactory;
	class ICommandBuffer;
	class IMaterialManager;
	class IRenderPass;

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

		inline std::vector<const IRenderPass*> GetRenderPasses() const
		{
			std::vector<const IRenderPass*> passes(m_renderPasses.size());
			for (size_t i = 0; i < m_renderPasses.size(); ++i)
			{
				passes[i] = m_renderPasses[i].get();
			}
			return passes;
		}

		inline const glm::vec2& GetTAAJitter(const glm::vec2& size)
		{
			return m_taaJitterOffsets[++m_taaFrameIndex % m_taaJitterOffsets.size()];
		}

		inline const IImageView& GetTAAPrevImageView() const { return m_taaPreviousImages[1]->GetView(); };

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
		std::vector<std::unique_ptr<IRenderPass>> m_renderPasses;
		Material* m_taaMaterial;
	};
}