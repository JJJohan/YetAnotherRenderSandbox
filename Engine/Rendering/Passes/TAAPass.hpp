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

		virtual bool Build(const Renderer& renderer, const std::unordered_map<const char*, IRenderImage*>& imageInputs,
			const std::unordered_map<const char*, IBuffer*>& bufferInputs) override;

		virtual void Draw(const IDevice& device, const ICommandBuffer& commandBuffer, 
			const glm::uvec2& size, uint32_t frameIndex, uint32_t passIndex) override;

		virtual void PreDraw(const IDevice& device, const ICommandBuffer& commandBuffer,
			const glm::uvec2& size, uint32_t frameIndex) override;

		virtual void PostDraw(const IDevice& device, const ICommandBuffer& commandBuffer,
			const glm::uvec2& size, uint32_t frameIndex) override;

	private:
		bool CreateTAAImage(const IDevice& device, const IResourceFactory& resourceFactory, const glm::uvec2& size);

		std::array<std::unique_ptr<IRenderImage>, 2> m_taaPreviousImages;
	};
}