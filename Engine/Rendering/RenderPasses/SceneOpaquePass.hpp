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
			const std::unordered_map<std::string, IRenderImage*>& imageInputs,
			const std::unordered_map<std::string, IRenderImage*>& imageOutputs,
			const std::unordered_map<std::string, IBuffer*>& bufferInputs,
			const std::unordered_map<std::string, IBuffer*>& bufferOutputs) override;

		virtual void UpdatePlaceholderFormats(Format swapchainFormat, Format depthFormat) override;

		virtual void Draw(const IDevice& device, const ICommandBuffer& commandBuffer,
			const glm::uvec2& size, uint32_t frameIndex, uint32_t layerIndex) override;

	private:
		const GeometryBatch& m_sceneGeometryBatch;
		bool m_built;
		IBuffer* m_indirectDrawBuffer;
	};
}