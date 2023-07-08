#pragma once

#include "IRenderPass.hpp"

namespace Engine::Rendering
{
	class GeometryBatch;

	class SceneOpaquePass : public IRenderPass
	{
	public:
		SceneOpaquePass(const GeometryBatch& sceneGeometryBatch);

		virtual bool Build(const Renderer& renderer, const std::unordered_map<const char*, IRenderImage*>& imageInputs,
			const std::unordered_map<const char*, IBuffer*>& bufferInputs) override;

		virtual void Draw(const IDevice& device, const ICommandBuffer& commandBuffer, uint32_t frameIndex) const override;

	private:
		const GeometryBatch& m_sceneGeometryBatch;
		bool m_built;
	};
}