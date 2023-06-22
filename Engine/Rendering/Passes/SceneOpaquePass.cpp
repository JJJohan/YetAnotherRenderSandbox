#include "SceneOpaquePass.hpp"
#include "../Resources/IBuffer.hpp"
#include "../Resources/IRenderImage.hpp"
#include "../Resources/IDevice.hpp"
#include "../Resources/ICommandBuffer.hpp"

namespace Engine::Rendering
{
	SceneOpaquePass::SceneOpaquePass()
	{
		m_imageResourceMap = {
			{"Albedo", nullptr},
			{"WorldNormal", nullptr},
			{"WorldPos", nullptr},
			{"MetalRoughness", nullptr},
			{"Velocity", nullptr},
			{"Depth", nullptr}
		};
	}

	void SceneOpaquePass::Draw(const IDevice& device, const ICommandBuffer& commandBuffer) const
	{
		// TODO
	}
}