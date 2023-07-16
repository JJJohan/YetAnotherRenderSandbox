#pragma once

#include "IRenderPass.hpp"

namespace Engine::Rendering
{
	class GeometryBatch;

	class SceneShadowPass : public IRenderPass
	{
	public:
		SceneShadowPass(const GeometryBatch& sceneGeometryBatch);

		virtual bool Build(const Renderer& renderer, const std::unordered_map<const char*, IRenderImage*>& imageInputs,
			const std::unordered_map<const char*, IBuffer*>& bufferInputs) override;

		virtual void Draw(const IDevice& device, const ICommandBuffer& commandBuffer,
			const glm::uvec2& size, uint32_t frameIndex, uint32_t layerIndex) override;

		virtual inline bool GetCustomSize(glm::uvec2& outSize) const override 
		{
			outSize = m_shadowResolution;
			return true; 
		}

	private:
		const GeometryBatch& m_sceneGeometryBatch;
		glm::uvec2 m_shadowResolution;
		bool m_built;
	};
}