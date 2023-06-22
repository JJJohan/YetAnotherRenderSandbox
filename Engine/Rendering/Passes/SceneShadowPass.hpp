#pragma once

#include "IRenderPass.hpp"

namespace Engine::Rendering
{
	class SceneShadowPass : public IRenderPass
	{
	public:
		SceneShadowPass();

		virtual void Draw(const IDevice& device, const ICommandBuffer& commandBuffer) const override;
	};
}