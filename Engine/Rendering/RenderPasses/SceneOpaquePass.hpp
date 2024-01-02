#pragma once

#include "IRenderPass.hpp"

namespace Engine::Rendering
{
	class GeometryBatch;

	class SceneOpaquePass : public IRenderPass
	{
	public:
		SceneOpaquePass(const GeometryBatch& sceneGeometryBatch);

		virtual bool Build(const Renderer& renderer,
			const std::unordered_map<const char*, IRenderImage*>& imageInputs,
			const std::unordered_map<const char*, IRenderImage*>& imageOutputs) override;

		virtual void UpdatePlaceholderFormats(Format swapchainFormat, Format depthFormat) override;

		virtual void Draw(const IDevice& device, const ICommandBuffer& commandBuffer,
			const glm::uvec2& size, uint32_t frameIndex, uint32_t layerIndex) override;

	private:
		const GeometryBatch& m_sceneGeometryBatch;
		bool m_built;
	};
}