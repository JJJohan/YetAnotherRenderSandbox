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

	class IComputePass : public IRenderNode
	{
	public:
		virtual ~IComputePass() = default;

		inline bool Initialise(const IMaterialManager& materialManager)
		{
			if (m_materialName != nullptr && !materialManager.TryGetMaterial(m_materialName, &m_material))
			{
				Engine::Logging::Logger::Error("Failed to find material '{}' for compute pass '{}'.", m_materialName, GetName());
				return false;
			}

			return true;
		}

		virtual bool Build(const Renderer& renderer) = 0;

		virtual void Dispatch(const IDevice& device, const ICommandBuffer& commandBuffer, 
			uint32_t frameIndex) = 0;

		virtual inline bool GetCustomSize(glm::uvec2& outSize) const { return false; }

		inline Material* GetMaterial() const { return m_material; }

	protected:
		IComputePass(const char* name, const char* materialName)
			: IRenderNode(name, RenderNodeType::Compute)
			, m_material(nullptr)
			, m_materialName(materialName)
		{
		}

		Material* m_material;

	private:
		const char* m_materialName;
	};
}