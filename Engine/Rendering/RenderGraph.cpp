#include "RenderGraph.hpp"
#include "Renderer.hpp"
#include "Passes/IRenderPass.hpp"
#include "Core/Logging/Logger.hpp"

using namespace Engine::Logging;

namespace Engine::Rendering
{
	RenderGraph::RenderGraph()
		: m_bufferResourceNodeLookup()
		, m_imageResourceNodeLookup()
		, m_renderGraph()
		, m_renderPasses()
	{
	}

	bool RenderGraph::AddPass(const IRenderPass* renderPass)
	{
		if (m_renderPasses.contains(renderPass->GetName()))
		{
			Logger::Error("Render pass with name '{}' already exists in the render graph.", renderPass->GetName());
			return false;
		}

		for (const char* bufferResourceName : renderPass->GetBufferOutputs())
		{
			if (m_bufferResourceNodeLookup.contains(bufferResourceName))
			{
				Logger::Error("Attempted to add a resource with the name '{}' which already exists in the render graph.", bufferResourceName);
				return false;
			}

			m_bufferResourceNodeLookup[bufferResourceName] = renderPass;
		}

		for (const char* imageResourceName : renderPass->GetImageOutputs())
		{
			if (m_imageResourceNodeLookup.contains(imageResourceName))
			{
				Logger::Error("Attempted to add a resource with the name '{}' which already exists in the render graph.", imageResourceName);
				return false;
			}

			m_imageResourceNodeLookup[imageResourceName] = renderPass;
		}

		m_renderPasses[renderPass->GetName()] = renderPass;
		return true;
	}

	bool RenderGraph::Build()
	{
		m_renderGraph.clear();
		std::vector<const IRenderPass*> renderPassStack;
		renderPassStack.reserve(m_renderPasses.size());
		for (const auto& pass : m_renderPasses)
		{
			renderPassStack.emplace_back(pass.second);
		}

		std::unordered_map<const char*, const IRenderPass*> availableBufferSources{};
		std::unordered_map<const char*, const IRenderPass*> availableImageSources{};

		while (!renderPassStack.empty())
		{
			std::vector<RenderNode>& stage = m_renderGraph.emplace_back(std::vector<RenderNode>());

			// Attempt to satisfy all requirements for each remaining render pass. If all requirements are met for a pass, remove it from the stack.
			for (auto it = renderPassStack.begin(); it != renderPassStack.end();)
			{
				const IRenderPass* renderPass = *it;
				RenderNode node(renderPass);
				bool satisfied = true;

				for (const char* bufferResourceName : renderPass->GetBufferInputs())
				{
					const auto& search = availableBufferSources.find(bufferResourceName);
					if (search != availableBufferSources.end())
					{
						node.InputBuffers[bufferResourceName] = search->second;
					}
					else
					{
						satisfied = false;
						break;
					}
				}

				if (satisfied)
				{
					for (const char* imageResourceName : renderPass->GetImageInputs())
					{
						const auto& search = availableImageSources.find(imageResourceName);
						if (search != availableImageSources.end())
						{
							node.InputImages[imageResourceName] = search->second;
						}
						else
						{
							// Special case for feedback loop.
							bool feedbackLoop = false;
							for (const char* output : renderPass->GetImageOutputs())
							{
								if (strcmp(output, imageResourceName) == 0)
								{
									feedbackLoop = true;
									node.InputImages[imageResourceName] = renderPass;
									break;
								}
							}

							if (!feedbackLoop)
							{
								satisfied = false;
								break;
							}
						}
					}
				}

				if (satisfied)
				{
					stage.emplace_back(std::move(node));
					it = renderPassStack.erase(it);
				}
				else
				{
					++it;
				}
			}

			if (stage.empty())
			{
				Logger::Error("Could not resolve requirements for remaining render passes while building render graph.");
				return false;
			}

			// Make resources from current stage available for the next one.
			for (const RenderNode& node : stage)
			{
				for (const char* bufferName : node.Pass->GetBufferOutputs())
					availableBufferSources[bufferName] = node.Pass;

				for (const char* imageName : node.Pass->GetImageOutputs())
					availableImageSources[imageName] = node.Pass;
			}
		}

		if (!availableImageSources.contains("Final"))
		{
			Logger::Error("Render graph should contain an image entry named 'Final' to be presented at end of frame.");
			return false;
		}

		return true;
	}

	void RenderGraph::Draw(const Renderer& renderer) const
	{
	}
}