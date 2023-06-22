#include "CombinePass.hpp"
#include "../Resources/IBuffer.hpp"
#include "../Resources/IRenderImage.hpp"
#include "../Resources/IDevice.hpp"
#include "../Resources/ICommandBuffer.hpp"

namespace Engine::Rendering
{
	CombinePass::CombinePass()
	{
		m_imageInputs =
		{
			"Albedo",
			"WorldNormal",
			"WorldPos",
			"MetalRoughness",
			"Velocity",
			"Shadows"
		};

		m_imageResourceMap = {
			{"Combined", nullptr}
		};
	}

	void CombinePass::Draw(const IDevice& device, const ICommandBuffer& commandBuffer) const
	{
		// TODO
	}
}