#pragma once

#include "IRenderPass.hpp"
#include <glm/glm.hpp>
#include "../Resources/IImageView.hpp"
#include "../Resources/IRenderImage.hpp"
#include "../Types.hpp"

namespace Engine::Rendering
{
	class ShadowMap;
	class IResourceFactory;

	class CombinePass : public IRenderPass
	{
	public:
		CombinePass(const ShadowMap& shadowMap);

		virtual bool Build(const Renderer& renderer,
			const std::unordered_map<const char*, IRenderImage*>& imageInputs,
			const std::unordered_map<const char*, IRenderImage*>& imageOutputs) override;

		virtual void Draw(const IDevice& device, const ICommandBuffer& commandBuffer, 
			const glm::uvec2& size, uint32_t frameIndex, uint32_t layerIndex) override;

		void SetDebugMode(uint32_t value) const;

	private:
		const ShadowMap& m_shadowMap;
	};
}