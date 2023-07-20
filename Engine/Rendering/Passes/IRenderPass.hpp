#pragma once

#include <vector>
#include <unordered_map>
#include "../IMaterialManager.hpp"
#include "Core/Logging/Logger.hpp"
#include "../Resources/Material.hpp"
#include "../Resources/AttachmentInfo.hpp"
#include "../RenderResources/IRenderResource.hpp"
#include "../RenderResources/IRenderNode.hpp"
#include <glm/glm.hpp>

namespace Engine::Rendering
{
	class IBuffer;
	class IRenderImage;
	class IDevice;
	class ICommandBuffer;
	class Renderer;

	struct RenderPassImageInfo
	{
		Format Format;
		bool IsRead;
		glm::uvec3 Dimensions;

		RenderPassImageInfo(Engine::Rendering::Format format = Format::Undefined, bool isRead = false, const glm::uvec3& dimensions = {})
			: Format(format)
			, IsRead(isRead)
			, Dimensions(dimensions)
		{
		}
	};

	class IRenderPass : public IRenderNode
	{
	public:
		virtual ~IRenderPass() = default;

		inline bool Initialise(const IMaterialManager& materialManager)
		{
			if (m_materialName != nullptr && !materialManager.TryGetMaterial(m_materialName, &m_material))
			{
				Engine::Logging::Logger::Error("Failed to find material '{}' for render pass '{}'.", m_materialName, GetName());
				return false;
			}

			return true;
		}

		virtual bool Build(const Renderer& renderer,
			const std::unordered_map<const char*, IRenderImage*>& imageInputs,
			const std::unordered_map<const char*, IRenderImage*>& imageOutputs) = 0;

		// Call before BeginRendering, but after command buffer Begin.
		inline virtual void PreDraw(const IDevice& device, const ICommandBuffer& commandBuffer,
			const glm::uvec2& size, uint32_t frameIndex, const std::unordered_map<const char*, IRenderImage*>& imageInputs,
			const std::unordered_map<const char*, IRenderImage*>& imageOutputs)
		{
		}

		virtual void Draw(const IDevice& device, const ICommandBuffer& commandBuffer, 
			const glm::uvec2& size, uint32_t frameIndex, uint32_t passIndex) = 0;

		// Called after EndRendering, but before command buffer End.
		inline virtual void PostDraw(const IDevice& device, const ICommandBuffer& commandBuffer,
			const glm::uvec2& size, uint32_t frameIndex, const std::unordered_map<const char*, IRenderImage*>& imageInputs,
			const std::unordered_map<const char*, IRenderImage*>& imageOutputs)
		{
		}

		virtual inline void ClearResources() override
		{
			m_colourAttachments.clear();
			m_depthAttachment.reset();
		}

		inline uint32_t GetLayerCount() const { return m_layerCount; }

		inline const std::unordered_map<const char*, IBuffer*>& GetBufferInputs() const { return m_bufferInputs; }

		inline const std::unordered_map<const char*, IBuffer*>& GetBufferOutputs() const { return m_bufferOutputs; }

		inline const std::unordered_map<const char*, RenderPassImageInfo>& GetImageInputInfos() const { return m_imageInputInfos; }

		inline const std::unordered_map<const char*, RenderPassImageInfo>& GetImageOutputInfos() const { return m_imageOutputInfos; }

		inline const std::vector<AttachmentInfo>& GetColourAttachments() const { return m_colourAttachments; }

		inline const std::optional<AttachmentInfo>& GetDepthAttachment() const { return m_depthAttachment; }

		inline void SetEnabled(bool enabled) { m_enabled = enabled; }

		inline bool GetEnabled() const { return m_enabled;  }

		virtual inline bool GetCustomSize(glm::uvec2& outSize) const { return false; }

		inline virtual RenderNodeType GetNodeType() const override { return RenderNodeType::Pass; }

	protected:
		IRenderPass(const char* name, const char* materialName)
			: IRenderNode(name)
			, m_bufferInputs()
			, m_bufferOutputs()
			, m_imageInputInfos()
			, m_imageOutputInfos()
			, m_layerCount(1)
			, m_material(nullptr)
			, m_materialName(materialName)
			, m_colourAttachments()
			, m_depthAttachment()
			, m_enabled(true)
		{
		}

		std::unordered_map<const char*, IBuffer*> m_bufferInputs;
		std::unordered_map<const char*, IBuffer*> m_bufferOutputs;
		std::unordered_map<const char*, RenderPassImageInfo> m_imageInputInfos;
		std::unordered_map<const char*, RenderPassImageInfo> m_imageOutputInfos;
		std::vector<AttachmentInfo> m_colourAttachments;
		std::optional<AttachmentInfo> m_depthAttachment;
		Material* m_material;
		uint32_t m_layerCount;
		bool m_enabled;

	private:
		const char* m_materialName;
	};
}