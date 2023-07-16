#pragma once

#include <vector>
#include <unordered_map>
#include "../IMaterialManager.hpp"
#include "Core/Logging/Logger.hpp"
#include "../Resources/Material.hpp"
#include "../Resources/AttachmentInfo.hpp"
#include "../RenderResources/IRenderResource.hpp"
#include <glm/glm.hpp>

namespace Engine::Rendering
{
	class IBuffer;
	class IRenderImage;
	class IDevice;
	class ICommandBuffer;
	class Renderer;

	class IRenderPass : public IRenderResource
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

		virtual bool Build(const Renderer& renderer, const std::unordered_map<const char*, IRenderImage*>& imageInputs,
			const std::unordered_map<const char*, IBuffer*>& bufferInputs) override
		{
			m_imageInputs = imageInputs;
			m_bufferInputs = bufferInputs;

			for (auto& imageOutput : m_imageOutputs)
			{
				const auto& input = imageInputs.find(imageOutput.first);
				if (input != imageInputs.end())
				{
					imageOutput.second = input->second;
				}
			}

			for (auto& bufferOutput : m_bufferOutputs)
			{
				const auto& input = bufferInputs.find(bufferOutput.first);
				if (input != bufferInputs.end())
				{
					bufferOutput.second = input->second;
				}
			}

			return true;
		}

		// Call before BeginRendering, but after command buffer Begin.
		inline virtual void PreDraw(const IDevice& device, const ICommandBuffer& commandBuffer,
			const glm::uvec2& size, uint32_t frameIndex) 
		{
		}

		virtual void Draw(const IDevice& device, const ICommandBuffer& commandBuffer, 
			const glm::uvec2& size, uint32_t frameIndex, uint32_t passIndex) = 0;

		// Called after EndRendering, but before command buffer End.
		inline virtual void PostDraw(const IDevice& device, const ICommandBuffer& commandBuffer,
			const glm::uvec2& size, uint32_t frameIndex)
		{
		}

		inline uint32_t GetLayerCount() const { return m_layerCount; }

		inline const std::unordered_map<const char*, IBuffer*>& GetBufferInputs() const { return m_bufferInputs; }

		inline const std::unordered_map<const char*, IRenderImage*>& GetImageInputs() const { return m_imageInputs; }

		inline const std::vector<AttachmentInfo>& GetColourAttachments() const { return m_colourAttachments; }

		inline const std::optional<AttachmentInfo>& GetDepthAttachment() const { return m_depthAttachment; }

		inline void SetEnabled(bool enabled) { m_enabled = enabled; }

		inline bool GetEnabled() const { return m_enabled;  }

		virtual inline bool GetCustomSize(glm::uvec2& outSize) const { return false; }

	protected:
		IRenderPass(const char* name, const char* materialName)
			: IRenderResource(name)
			, m_bufferInputs()
			, m_imageInputs()
			, m_layerCount(1)
			, m_material(nullptr)
			, m_materialName(materialName)
			, m_colourAttachments()
			, m_depthAttachment()
			, m_enabled(true)
		{
		}

		std::unordered_map<const char*, IBuffer*> m_bufferInputs;
		std::unordered_map<const char*, IRenderImage*> m_imageInputs;
		std::vector<AttachmentInfo> m_colourAttachments;
		std::optional<AttachmentInfo> m_depthAttachment;
		Material* m_material;
		uint32_t m_layerCount;
		bool m_enabled;

	private:
		const char* m_materialName;
	};
}