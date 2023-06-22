#include "RenderGraph.hpp"
#include "Renderer.hpp"
#include "Passes/IRenderPass.hpp"
#include "Resources/IBuffer.hpp"
#include "Resources/IRenderImage.hpp"

namespace Engine::Rendering
{
	RenderGraph::RenderGraph()
		: m_bufferResourceMap()
		, m_imageResourceMap()
		, m_renderGraph()
		, m_renderPasses()
	{
	}

	bool RenderGraph::AddPass(const IRenderPass* renderPass)
	{
		// Map all the available resources from the pass, ensuring there's no duplicate resource names.
		return false;
	}

	bool RenderGraph::Build()
	{
		return false;
	}

	void RenderGraph::Draw(const Renderer& renderer) const
	{
	}
}