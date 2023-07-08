#include "RenderGraph.hpp"
#include "Renderer.hpp"
#include "Core/Logging/Logger.hpp"
#include "IMaterialManager.hpp"
#include <unordered_set>

using namespace Engine::Logging;

namespace Engine::Rendering
{
	RenderGraph::RenderGraph()
		: m_bufferResourceNodeLookup()
		, m_imageResourceNodeLookup()
		, m_renderGraph()
		, m_renderPasses()
		, m_dirty(true)
	{
	}

	bool RenderGraph::AddPass(IRenderPass* renderPass, const IMaterialManager& materialManager)
	{
		if (m_renderPasses.contains(renderPass->GetName()))
		{
			Logger::Error("Render pass with name '{}' already exists in the render graph.", renderPass->GetName());
			return false;
		}

		std::unordered_set<const char*> inputBuffers(renderPass->GetBufferInputs().begin(), renderPass->GetBufferInputs().end());
		std::unordered_set<const char*> inputImages(renderPass->GetImageInputs().begin(), renderPass->GetImageInputs().end());

		for (const char* bufferResourceName : renderPass->GetBufferOutputs())
		{
			if (m_bufferResourceNodeLookup.contains(bufferResourceName))
			{
				if (inputBuffers.contains(bufferResourceName))
				{
					// Pass-through resource.
					continue;
				}

				Logger::Error("Attempted to add a resource with the name '{}' which already exists in the render graph.", bufferResourceName);
				return false;
			}

			m_bufferResourceNodeLookup[bufferResourceName] = renderPass;
		}

		for (const char* imageResourceName : renderPass->GetImageOutputs())
		{
			if (m_imageResourceNodeLookup.contains(imageResourceName))
			{
				if (inputImages.contains(imageResourceName))
				{
					// Pass-through resource.
					continue;
				}

				Logger::Error("Attempted to add a resource with the name '{}' which already exists in the render graph.", imageResourceName);
				return false;
			}

			m_imageResourceNodeLookup[imageResourceName] = renderPass;
		}

		if (!renderPass->Initialise(materialManager))
			return false;

		m_renderPasses[renderPass->GetName()] = renderPass;
		return true;
	}

	bool RenderGraph::AddResource(IRenderResource* renderResource)
	{
		if (m_renderResources.contains(renderResource->GetName()))
		{
			Logger::Error("Render resource with name '{}' already exists in the render graph.", renderResource->GetName());
			return false;
		}

		for (const char* bufferResourceName : renderResource->GetBufferOutputs())
		{
			if (m_bufferResourceNodeLookup.contains(bufferResourceName))
			{
				Logger::Error("Attempted to add a resource with the name '{}' which already exists in the render graph.", bufferResourceName);
				return false;
			}

			m_bufferResourceNodeLookup[bufferResourceName] = renderResource;
		}

		for (const char* imageResourceName : renderResource->GetImageOutputs())
		{
			if (m_imageResourceNodeLookup.contains(imageResourceName))
			{
				Logger::Error("Attempted to add a resource with the name '{}' which already exists in the render graph.", imageResourceName);
				return false;
			}

			m_imageResourceNodeLookup[imageResourceName] = renderResource;
		}

		m_renderResources[renderResource->GetName()] = renderResource;
		return true;
	}

	bool RenderGraph::Build(const Renderer& renderer)
	{
		m_renderGraph.clear();

		std::unordered_map<const char*, const IRenderResource*> availableBufferSources{};
		std::unordered_map<const char*, const IRenderResource*> availableImageSources{};

		if (!m_renderResources.empty())
		{
			std::vector<RenderNode>& stage = m_renderGraph.emplace_back(std::vector<RenderNode>());

			for (auto& renderResource : m_renderResources)
			{
				for (const char* bufferName : renderResource.second->GetBufferOutputs())
					availableBufferSources[bufferName] = renderResource.second;

				for (const char* imageName : renderResource.second->GetImageOutputs())
					availableImageSources[imageName] = renderResource.second;

				RenderNode node(renderResource.second);
				stage.emplace_back(node);
			}
		}

		std::vector<IRenderPass*> renderPassStack;
		renderPassStack.reserve(m_renderPasses.size());
		for (const auto& pass : m_renderPasses)
		{
			renderPassStack.emplace_back(pass.second);
		}

		while (!renderPassStack.empty())
		{
			std::vector<RenderNode>& stage = m_renderGraph.emplace_back(std::vector<RenderNode>());

			// Per-stage available resources.
			std::unordered_map<const char*, const IRenderResource*> stageAvailableBufferSources(availableBufferSources);
			std::unordered_map<const char*, const IRenderResource*> stageAvailableImageSources(availableImageSources);

			// Attempt to satisfy all requirements for each remaining render pass. If all requirements are met for a pass, remove it from the stack.
			for (auto it = renderPassStack.begin(); it != renderPassStack.end();)
			{
				IRenderPass* renderPass = *it;
				RenderNode node(renderPass);
				bool satisfied = true;

				for (const char* bufferResourceName : renderPass->GetBufferInputs())
				{
					const auto& search = stageAvailableBufferSources.find(bufferResourceName);
					if (search != stageAvailableBufferSources.end())
					{
						// Make resource unavailable for the rest of this stage if it's being written to (exists in output.)
						node.InputBuffers[bufferResourceName] = search->second;
						for (const char* bufferOutput : renderPass->GetBufferOutputs())
						{
							if (strcmp(bufferOutput, search->first) == 0)
							{
								stageAvailableBufferSources.erase(bufferOutput);
								break;
							}
						}
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
						const auto& search = stageAvailableImageSources.find(imageResourceName);
						if (search != stageAvailableImageSources.end())
						{
							// Make resource unavailable for the rest of this stage if it's being written to (exists in output.)
							node.InputImages[imageResourceName] = search->second;
							for (const char* imageOutput : renderPass->GetImageOutputs())
							{
								if (strcmp(imageOutput, search->first) == 0)
								{
									stageAvailableImageSources.erase(imageOutput);
									break;
								}
							}
						}
						else
						{
							satisfied = false;
							break;
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

		// Build the actual passes.
		for (const auto& stage : m_renderGraph)
		{
			for (const auto& node : stage)
			{
				std::unordered_map<const char*, IRenderImage*> inputImages(node.InputImages.size());
				for (auto& inputImageSource : node.InputImages)
				{
					inputImages[inputImageSource.first] = inputImageSource.second->GetImageResource(inputImageSource.first);
				}

				std::unordered_map<const char*, IBuffer*> inputBuffers(node.InputBuffers.size());
				for (auto& inputBufferSource : node.InputBuffers)
				{
					inputBuffers[inputBufferSource.first] = inputBufferSource.second->GetBufferResource(inputBufferSource.first);
				}

				if (node.Pass != nullptr)
				{
					if (!node.Pass->Build(renderer, inputImages, inputBuffers))
					{
						Logger::Error("Failed to build render resource '{}' while building render graph.", node.Pass->GetName());
						return false;
					}
				}
				else
				{
					if (!node.Resource->Build(renderer))
					{
						Logger::Error("Failed to build render resource '{}' while building render graph.", node.Resource->GetName());
						return false;
					}

				}
			}
		}

		m_dirty = false;
		return true;
	}

	void RenderGraph::Draw(const Renderer& renderer) const
	{
	}
}