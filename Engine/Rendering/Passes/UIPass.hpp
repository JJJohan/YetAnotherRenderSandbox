#pragma once

#include "IRenderPass.hpp"
#include <array>
#include <glm/glm.hpp>
#include <memory>

namespace Engine::UI
{
	class UIManager;
}

namespace Engine::Rendering
{
	class IResourceFactory;

	class UIPass : public IRenderPass
	{
	public:
		UIPass(Engine::UI::UIManager& uiManager);

		virtual bool Build(const Renderer& renderer,
			const std::unordered_map<const char*, IRenderImage*>& imageInputs,
			const std::unordered_map<const char*, IRenderImage*>& imageOutputs) override;

		virtual void Draw(const IDevice& device, const ICommandBuffer& commandBuffer, 
			const glm::uvec2& size, uint32_t frameIndex, uint32_t layerIndex) override;

	private:
		Engine::UI::UIManager& m_uiManager;
	};
}