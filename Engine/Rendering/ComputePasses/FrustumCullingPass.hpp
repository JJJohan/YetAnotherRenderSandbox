#pragma once

#include "IComputePass.hpp"

namespace Engine::Rendering
{
	class IResourceFactory;
	class GeometryBatch;

	class FrustumCullingPass : public IComputePass
	{
	public:
		FrustumCullingPass(const GeometryBatch& sceneGeometryBatch);

		virtual bool Build(const Renderer& renderer) override;

		virtual void Dispatch(const IDevice& device, const ICommandBuffer& commandBuffer,
			uint32_t frameIndex) override;

	private:
		const GeometryBatch& m_sceneGeometryBatch;
		bool m_built;
		uint32_t m_dispatchSize;
	};
}