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

		// Temp
		inline void SetShadowCascadeIndex(int index) { m_cascadeIndex = index; }

		virtual void Draw(const IDevice& device, const ICommandBuffer& commandBuffer, uint32_t frameIndex) const override;

	private:
		uint32_t m_cascadeIndex;
		const GeometryBatch& m_sceneGeometryBatch;
		bool m_built;
	};
}