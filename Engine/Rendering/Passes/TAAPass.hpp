#pragma once

#include "IRenderPass.hpp"

namespace Engine::Rendering
{
	class TAAPass : public IRenderPass
	{
	public:
		TAAPass();

		virtual void Draw(const IDevice& device, const ICommandBuffer& commandBuffer) const override;
	};
}