#pragma once

#include "../IRenderPass.hpp"
#include <array>
#include <glm/glm.hpp>
#include <memory>

namespace Engine::Rendering
{
	class IResourceFactory;

	class SMAAEdgesPass : public IRenderPass
	{
	public:
		SMAAEdgesPass();

		virtual bool Build(const Renderer& renderer,
			const std::unordered_map<std::string, IRenderImage*>& imageInputs,
			const std::unordered_map<std::string, IRenderImage*>& imageOutputs,
			const std::unordered_map<std::string, IBuffer*>& bufferInputs,
			const std::unordered_map<std::string, IBuffer*>& bufferOutputs) override;

		virtual void UpdatePlaceholderFormats(Format swapchainFormat, Format depthFormat) override;

		virtual void Draw(const Renderer& renderer, const ICommandBuffer& commandBuffer,
			const glm::uvec2& size, uint32_t frameIndex, uint32_t passIndex) override;
	};
}