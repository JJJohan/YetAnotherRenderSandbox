#include "SceneOpaquePass.hpp"
#include "../Resources/IBuffer.hpp"
#include "../Resources/IRenderImage.hpp"
#include "../Resources/IDevice.hpp"
#include "../Resources/ICommandBuffer.hpp"

namespace Engine::Rendering
{
	SceneOpaquePass::SceneOpaquePass()
	{
		m_imageOutputs =
		{
			"Albedo",
			"WorldNormal",
			"WorldPos",
			"MetalRoughness",
			"Velocity",
			"Depth"
		};
	}

	void SceneOpaquePass::Draw(const IDevice& device, const ICommandBuffer& commandBuffer) const
	{
		// TODO
	}

	const IRenderImage& SceneOpaquePass::GetImageResource(const char* name) const
	{
		// TODO
		throw;
	}

	const IBuffer& SceneOpaquePass::GetBufferResource(const char* name) const
	{
		// TODO
		throw;
	}
}