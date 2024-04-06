#pragma once

#include "../IRenderPass.hpp"
#include <array>
#include <glm/glm.hpp>
#include <memory>

namespace Engine::Rendering
{
	class IResourceFactory;
	class IPhysicalDevice;

	class SMAAWeightsPass : public IRenderPass
	{
	public:
		SMAAWeightsPass();

		virtual bool Build(const Renderer& renderer,
			const std::unordered_map<std::string, IRenderImage*>& imageInputs,
			const std::unordered_map<std::string, IRenderImage*>& imageOutputs,
			const std::unordered_map<std::string, IBuffer*>& bufferInputs,
			const std::unordered_map<std::string, IBuffer*>& bufferOutputs) override;

		virtual void PreDraw(const Renderer& renderer, const ICommandBuffer& commandBuffer,
			const glm::uvec2& size, uint32_t frameIndex, const std::unordered_map<std::string, IRenderImage*>& imageInputs,
			const std::unordered_map<std::string, IRenderImage*>& imageOutputs) override;

		virtual void Draw(const Renderer& renderer, const ICommandBuffer& commandBuffer,
			const glm::uvec2& size, uint32_t frameIndex, uint32_t passIndex) override;

		virtual void ClearResources() override;

	private:
		bool CreateLookupTextures(const IDevice& device, const IResourceFactory& resourceFactory);
		bool UploadLookupTextureData(const IDevice& device, const IPhysicalDevice& physicalDevice,
			const IResourceFactory& resourceFactory, const ICommandBuffer& commandBuffer);

		std::unique_ptr<IRenderImage> m_areaTexture;
		std::unique_ptr<IRenderImage> m_searchTexture;
		std::unique_ptr<IBuffer> m_areaUploadBuffer;
		std::unique_ptr<IBuffer> m_searchUploadBuffer;
		bool m_lookupTexturesUploaded;
	};
}