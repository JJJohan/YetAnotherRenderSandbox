#pragma once

#include <vector>
#include "../IMaterialManager.hpp"
#include "Core/Logger.hpp"
#include "../Resources/Material.hpp"
#include "../Resources/AttachmentInfo.hpp"
#include "../RenderResources/IRenderNode.hpp"

namespace Engine::Rendering
{
	class IRenderImage;
	class IDevice;
	class ICommandBuffer;
	class Renderer;

	class IRenderPass : public IRenderNode
	{
	public:
		virtual ~IRenderPass() = default;

		inline bool Initialise(const IMaterialManager& materialManager)
		{
			if (m_materialName != "" && !materialManager.TryGetMaterial(m_materialName, &m_material))
			{
				Engine::Logger::Error("Failed to find material '{}' for render pass '{}'.", m_materialName, GetName());
				return false;
			}

			return true;
		}

		// Call before BeginRendering, but after command buffer Begin.
		inline virtual void PreDraw(const Renderer& renderer, const ICommandBuffer& commandBuffer,
			const glm::uvec2& size, uint32_t frameIndex, const std::unordered_map<std::string, IRenderImage*>& imageInputs,
			const std::unordered_map<std::string, IRenderImage*>& imageOutputs)
		{
		}

		virtual void Draw(const Renderer& renderer, const ICommandBuffer& commandBuffer,
			const glm::uvec2& size, uint32_t frameIndex, uint32_t passIndex) = 0;

		// Called after EndRendering, but before command buffer End.
		inline virtual void PostDraw(const Renderer& renderer, const ICommandBuffer& commandBuffer,
			const glm::uvec2& size, uint32_t frameIndex, const std::unordered_map<std::string, IRenderImage*>& imageInputs,
			const std::unordered_map<std::string, IRenderImage*>& imageOutputs)
		{
		}

		virtual inline void ClearResources() override
		{
			m_colourAttachments.clear();
			m_depthAttachment.reset();
		}

		inline uint32_t GetLayerCount() const { return m_layerCount; }

		inline const std::vector<AttachmentInfo>& GetColourAttachments() const { return m_colourAttachments; }

		inline const std::optional<AttachmentInfo>& GetDepthAttachment() const { return m_depthAttachment; }

		virtual inline bool GetCustomSize(glm::uvec2& outSize) const { return false; }

		inline Material* GetMaterial() const { return m_material; }

	protected:
		IRenderPass(std::string_view name, std::string_view materialName)
			: IRenderNode(name, RenderNodeType::Pass)
			, m_layerCount(1)
			, m_material(nullptr)
			, m_materialName(materialName)
			, m_colourAttachments()
			, m_depthAttachment()
		{
		}

		std::vector<AttachmentInfo> m_colourAttachments;
		std::optional<AttachmentInfo> m_depthAttachment;
		Material* m_material;
		uint32_t m_layerCount;

	private:
		std::string m_materialName;
	};
}