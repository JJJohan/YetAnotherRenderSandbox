#include "CombinePass.hpp"
#include "../Resources/IBuffer.hpp"
#include "../Resources/IRenderImage.hpp"
#include "../Resources/IDevice.hpp"
#include "../Resources/ICommandBuffer.hpp"

namespace Engine::Rendering
{
	CombinePass::CombinePass()
	{
		m_name = "Combine";

		m_imageInputs =
		{
			"Albedo",
			"WorldNormal",
			"WorldPos",
			"MetalRoughness",
			"Shadows"
		};

		m_imageOutputs = 
		{
			"Combined"
		};
	}

	void CombinePass::Draw(const IDevice& device, const ICommandBuffer& commandBuffer) const
	{
		// TODO
	}

	const IRenderImage& CombinePass::GetImageResource(const char* name) const
	{
		// TODO
		throw;
	}

	const IBuffer& CombinePass::GetBufferResource(const char* name) const
	{
		// TODO
		throw;
	}
}