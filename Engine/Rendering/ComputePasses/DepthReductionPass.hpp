#pragma once

#include "IComputePass.hpp"

namespace Engine::Rendering
{
	class IResourceFactory;
	class FrustumCullingPass;

	class DepthReductionPass : public IComputePass
	{
	public:
		DepthReductionPass(FrustumCullingPass& frustumCullingPass);

		virtual bool Build(const Renderer& renderer,
			const std::unordered_map<std::string, IRenderImage*>& imageInputs,
			const std::unordered_map<std::string, IRenderImage*>& imageOutputs,
			const std::unordered_map<std::string, IBuffer*>& bufferInputs,
			const std::unordered_map<std::string, IBuffer*>& bufferOutputs) override;

		virtual void Dispatch(const Renderer& renderer, const ICommandBuffer& commandBuffer,
			uint32_t frameIndex) override;

		virtual inline void ClearResources() override
		{
			m_occlusionImage.reset();
			m_occlusionMipViews.clear();
		}

		inline IRenderImage* GetOcclusionImage() { return m_occlusionImage.get(); };

	private:
		bool CreateOcclusionImage(const IDevice& device, const IResourceFactory& resourceFactory);

		std::unique_ptr<IRenderImage> m_occlusionImage;
		std::vector<std::unique_ptr<IImageView>> m_occlusionMipViews;
		IRenderImage* m_depthImage;
		FrustumCullingPass& m_frustumCullingPass;
		uint32_t m_depthPyramidLevels;
		uint32_t m_depthPyramidWidth;
		uint32_t m_depthPyramidHeight;
	};
}