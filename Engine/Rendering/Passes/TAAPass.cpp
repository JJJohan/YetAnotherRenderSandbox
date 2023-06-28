#include "TAAPass.hpp"
#include "../Resources/IBuffer.hpp"
#include "../Resources/IRenderImage.hpp"
#include "../Resources/IDevice.hpp"
#include "../Resources/ICommandBuffer.hpp"

namespace Engine::Rendering
{
	TAAPass::TAAPass()
	{
		m_imageInputs = 
		{
			"Combined",
			"Velocity",
			"Depth",
			"History"
		};

		m_imageOutputs =
		{
			"Final",
			"History"
		};
	}

	void TAAPass::Draw(const IDevice& device, const ICommandBuffer& commandBuffer) const
	{
		// TODO
	}

	const IRenderImage& TAAPass::GetImageResource(const char* name) const
	{
		// TODO
		throw;
	}

	const IBuffer& TAAPass::GetBufferResource(const char* name) const
	{
		// TODO
		throw;
	}
}