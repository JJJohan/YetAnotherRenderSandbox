#pragma once

#include "IComputePass.hpp"
#include "../CullingMode.hpp"

namespace Engine::Rendering
{
	class IResourceFactory;
	class GeometryBatch;

	class FrustumCullingPass : public IComputePass
	{
	public:
		FrustumCullingPass(const GeometryBatch& sceneGeometryBatch);

		virtual bool Build(const Renderer& renderer,
			const std::unordered_map<const char*, IRenderImage*>& imageInputs,
			const std::unordered_map<const char*, IRenderImage*>& imageOutputs) override;

		bool FrustumPassBuild(const Renderer& renderer, IRenderImage* occlusionImage);

		virtual void Dispatch(const Renderer& renderer, const ICommandBuffer& commandBuffer,
			uint32_t frameIndex) override;

		void SetCullingMode(CullingMode mode);

	private:
		struct DrawCullData
		{
			float P00, P11, znear, zfar; // symmetric projection parameters
			glm::vec4 frustum; // data for left/right/top/bottom frustum planes
			float pyramidWidth, pyramidHeight; // depth pyramid size in texels
			uint32_t meshCount;
		};

		const GeometryBatch& m_sceneGeometryBatch;
		CullingMode m_mode;
		bool m_built;
		IRenderImage* m_occlusionImage;
		uint32_t m_dispatchSize;
		DrawCullData m_drawCullData;
	};
}