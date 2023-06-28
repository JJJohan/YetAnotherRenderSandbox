#include "SceneShadowPass.hpp"
#include "../Resources/IBuffer.hpp"
#include "../Resources/IRenderImage.hpp"
#include "../Resources/IDevice.hpp"
#include "../Resources/ICommandBuffer.hpp"

namespace Engine::Rendering
{
	SceneShadowPass::SceneShadowPass()
	{
		m_imageOutputs = 
		{
			"Shadows"
		};
	}

	void SceneShadowPass::Draw(const IDevice& device, const ICommandBuffer& commandBuffer) const
	{
		// TODO
	}

	const IRenderImage& SceneShadowPass::GetImageResource(const char* name) const
	{
		// TODO
		throw;
	}

	const IBuffer& SceneShadowPass::GetBufferResource(const char* name) const
	{
		// TODO
		throw;
	}
}