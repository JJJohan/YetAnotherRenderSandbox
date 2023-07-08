#pragma once

#include <vector>
#include <unordered_map>
#include "../IMaterialManager.hpp"
#include "Core/Logging/Logger.hpp"
#include "../Resources/Material.hpp"
#include "../RenderResources/IRenderResource.hpp"

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
			if (!materialManager.TryGetMaterial(m_materialName, &m_material))
			{
				Engine::Logging::Logger::Error("Failed to find material '{}' for render pass '{}'.", m_materialName, GetName());
				return false;
			}

			return true;
		}

		inline virtual bool Build(const Renderer& renderer) override { return false; }

		virtual bool Build(const Renderer& renderer, const std::unordered_map<const char*, IRenderImage*>& imageInputs,
			const std::unordered_map<const char*, IBuffer*>& bufferInputs) = 0;

		virtual void Draw(const IDevice& device, const ICommandBuffer& commandBuffer, uint32_t frameIndex) const = 0;

		inline float GetFrameTime() const { return m_frameTime; }

		inline void SetFrameTime(float time) { m_frameTime = time; }

		inline const std::vector<const char*>& GetBufferInputs() const { return m_bufferInputs; }

		inline const std::vector<const char*>& GetImageInputs() const { return m_imageInputs; }

	protected:
		IRenderPass(const char* name, const char* materialName)
			: IRenderResource(name)
			, m_bufferInputs()
			, m_imageInputs()
			, m_frameTime(0.0f)
			, m_material(nullptr)
			, m_materialName(materialName)
		{
		}

		std::vector<const char*> m_bufferInputs;
		std::vector<const char*> m_imageInputs;
		float m_frameTime;
		Material* m_material;

	private:
		const char* m_materialName;
	};
}