#pragma once

#include "IRenderPass.hpp"

namespace Engine::Rendering
{
	class SceneOpaquePass : public IRenderPass
	{
	public:
		SceneOpaquePass();

		virtual void Draw(const IDevice& device, const ICommandBuffer& commandBuffer) const override;
	};
}