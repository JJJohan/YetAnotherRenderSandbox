#pragma once

#include "IRenderPass.hpp"

namespace Engine::Rendering
{
	class IResourceFactory;

	class TonemapperPass : public IRenderPass
	{
	public:
		TonemapperPass();

		virtual bool Build(const Renderer& renderer,
			const std::unordered_map<std::string, IRenderImage*>& imageInputs,
			const std::unordered_map<std::string, IRenderImage*>& imageOutputs,
			const std::unordered_map<std::string, IBuffer*>& bufferInputs,
			const std::unordered_map<std::string, IBuffer*>& bufferOutputs) override;

		virtual void UpdatePlaceholderFormats(Format swapchainFormat, Format depthFormat) override;

		virtual void Draw(const IDevice& device, const ICommandBuffer& commandBuffer, 
			const glm::uvec2& size, uint32_t frameIndex, uint32_t passIndex) override;

		inline virtual void ClearResources() override
		{
			IRenderPass::ClearResources();
		}
	};
}