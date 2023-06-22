#include "SceneShadowPass.hpp"
#include "../Resources/IBuffer.hpp"
#include "../Resources/IRenderImage.hpp"
#include "../Resources/IDevice.hpp"
#include "../Resources/ICommandBuffer.hpp"

namespace Engine::Rendering
{
	SceneShadowPass::SceneShadowPass()
	{
		m_imageResourceMap = {
			{"Shadows", nullptr}
		};
	}

	void SceneShadowPass::Draw(const IDevice& device, const ICommandBuffer& commandBuffer) const
	{
		// TODO
	}
}