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
			const std::unordered_map<std::string, IRenderImage*>& imageInputs,
			const std::unordered_map<std::string, IRenderImage*>& imageOutputs,
			const std::unordered_map<std::string, IBuffer*>& bufferInputs,
			const std::unordered_map<std::string, IBuffer*>& bufferOutputs) override;

		bool FrustumPassBuild(const Renderer& renderer, IRenderImage* occlusionImage);

		virtual void Dispatch(const Renderer& renderer, const ICommandBuffer& commandBuffer,
			uint32_t frameIndex) override;

		virtual bool BuildResources(const Renderer& renderer) override;
		virtual void ClearResources() override;

		void SetCullingMode(CullingMode mode);

	private:
		struct DrawCullData
		{
			float P00, P11, znear, zfar; // symmetric projection parameters
			glm::vec4 frustum; // data for left/right/top/bottom frustum planes
			float pyramidWidth, pyramidHeight; // depth pyramid size in texels
			uint32_t enableOcclusion;
			uint32_t resetCounter;
		};

		bool BuildBuffers(const Renderer& renderer, uint32_t meshCount);
		bool CreateIndirectBuffer(const IResourceFactory& resourceFactory, uint32_t meshCount, std::unique_ptr<IBuffer>& outBuffer);

		const GeometryBatch& m_sceneGeometryBatch;
		CullingMode m_mode;
		bool m_built;
		IRenderImage* m_occlusionImage;
		uint32_t m_dispatchSize;
		DrawCullData m_drawCullData;
		std::unique_ptr<IBuffer> m_indirectBuffer;
		std::unique_ptr<IBuffer> m_shadowIndirectBuffer;
	};
}