#pragma once

#include "IRenderPass.hpp"

namespace Engine::Rendering
{
	class SceneOpaquePass : public IRenderPass
	{
	public:
		SceneOpaquePass();

		virtual void Draw(const IDevice& device, const ICommandBuffer& commandBuffer) const override;

		virtual const IRenderImage& GetImageResource(const char* name) const override;

		virtual const IBuffer& GetBufferResource(const char* name) const override;
	};
}