#pragma once

#include "IRenderPass.hpp"
#include <array>
#include <glm/glm.hpp>
#include <memory>

namespace Engine::Rendering
{
	class IResourceFactory;

	class TAAPass : public IRenderPass
	{
	public:
		TAAPass();

		virtual bool Build(const Renderer& renderer,
			const std::unordered_map<const char*, IRenderImage*>& imageInputs,
			const std::unordered_map<const char*, IRenderImage*>& imageOutputs) override;

		virtual void UpdatePlaceholderFormats(Format swapchainFormat, Format depthFormat) override;

		virtual void Draw(const IDevice& device, const ICommandBuffer& commandBuffer, 
			const glm::uvec2& size, uint32_t frameIndex, uint32_t passIndex) override;

		virtual void PreDraw(const IDevice& device, const ICommandBuffer& commandBuffer,
			const glm::uvec2& size, uint32_t frameIndex, const std::unordered_map<const char*, IRenderImage*>& imageInputs,
			const std::unordered_map<const char*, IRenderImage*>& imageOutputs) override;

		virtual void PostDraw(const IDevice& device, const ICommandBuffer& commandBuffer,
			const glm::uvec2& size, uint32_t frameIndex, const std::unordered_map<const char*, IRenderImage*>& imageInputs,
			const std::unordered_map<const char*, IRenderImage*>& imageOutputs) override;

		inline virtual void ClearResources() override
		{
			m_taaHistoryImage.reset();
			IRenderPass::ClearResources();
		}

	private:
		bool CreateTAAHistoryImage(const IDevice& device, const IResourceFactory& resourceFactory, const glm::uvec2& size);

		std::unique_ptr<IRenderImage> m_taaHistoryImage;
	};
}