#pragma once

#include "IRenderPass.hpp"

namespace Engine::Rendering
{
	class GeometryBatch;
	class ShadowMap;

	class SceneShadowPass : public IRenderPass
	{
	public:
		SceneShadowPass(const GeometryBatch& sceneGeometryBatch, const ShadowMap& shadowMap);

		virtual bool Build(const Renderer& renderer,
			const std::unordered_map<std::string, IRenderImage*>& imageInputs,
			const std::unordered_map<std::string, IRenderImage*>& imageOutputs,
			const std::unordered_map<std::string, IBuffer*>& bufferInputs,
			const std::unordered_map<std::string, IBuffer*>& bufferOutputs) override;

		virtual void UpdatePlaceholderFormats(Format swapchainFormat, Format depthFormat) override;

		virtual void Draw(const IDevice& device, const ICommandBuffer& commandBuffer,
			const glm::uvec2& size, uint32_t frameIndex, uint32_t layerIndex) override;

		virtual inline bool GetCustomSize(glm::uvec2& outSize) const override
		{
			outSize = m_shadowResolution;
			return true;
		}

	private:
		const GeometryBatch& m_sceneGeometryBatch;
		const ShadowMap& m_shadowMap;
		glm::uvec2 m_shadowResolution;
		IBuffer* m_indirectDrawBuffer;
		bool m_built;
	};
}