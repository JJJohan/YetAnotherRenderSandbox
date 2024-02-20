#pragma once

#include <vector>
#include <unordered_map>
#include "../IMaterialManager.hpp"
#include "Core/Logging/Logger.hpp"
#include "../Resources/Material.hpp"
#include "../RenderResources/IRenderResource.hpp"
#include "../RenderResources/IRenderNode.hpp"

namespace Engine::Rendering
{
	class IDevice;
	class ICommandBuffer;
	class Renderer;
	class RenderGraph;

	class IComputePass : public IRenderNode
	{
	public:
		virtual ~IComputePass() = default;

		inline bool Initialise(const IMaterialManager& materialManager)
		{
			if (m_materialName != "" && !materialManager.TryGetMaterial(m_materialName, &m_material))
			{
				Engine::Logging::Logger::Error("Failed to find material '{}' for compute pass '{}'.", m_materialName, GetName());
				return false;
			}

			return true;
		}

		virtual bool Build(const Renderer& renderer,
			const std::unordered_map<std::string, IRenderImage*>& imageInputs,
			const std::unordered_map<std::string, IRenderImage*>& imageOutputs,
			const std::unordered_map<std::string, IBuffer*>& bufferInputs,
			const std::unordered_map<std::string, IBuffer*>& bufferOutputs) = 0;

		virtual void Dispatch(const Renderer& renderer, const ICommandBuffer& commandBuffer, 
			uint32_t frameIndex) = 0;

		virtual inline bool GetCustomSize(glm::uvec2& outSize) const { return false; }

		inline Material* GetMaterial() const { return m_material; }

	protected:
		IComputePass(std::string_view name, std::string_view materialName)
			: IRenderNode(name, RenderNodeType::Compute)
			, m_material(nullptr)
			, m_materialName(materialName)
		{
		}

		Material* m_material;

	private:
		std::string m_materialName;
	};
}