#pragma once

#include <glm/glm.hpp>
#include "../Types.hpp"
#include <memory>
#include <vector>
#include "../Resources/IRenderImage.hpp"
#include "IRenderResource.hpp"

#define GBUFFER_SIZE 5 // albedo, normal, worldPos, metalRoughness, velocity

namespace Engine::Rendering
{
	class IPhysicalDevice;
	class IDevice;
	class IResourceFactory;
	struct AttachmentInfo;

	class GBuffer : public IRenderResource
	{
	public:
		GBuffer();

		virtual bool Build(const Renderer& renderer, const std::unordered_map<const char*, IRenderImage*>& imageInputs,
			const std::unordered_map<const char*, IBuffer*>& bufferInputs) override;
		
		virtual size_t GetMemoryUsage() const override;

	private:
		bool CreateImageAndView(const IDevice& device, const IResourceFactory& resourceFactory, const glm::uvec2& size, Format format);
		bool CreateColorImages(const IDevice& device, const IResourceFactory& resourceFactory, const glm::uvec2& size);
		bool CreateDepthImage(const IDevice& device, const IResourceFactory& resourceFactory, const glm::uvec2& size, Format format);

		std::vector<std::unique_ptr<IRenderImage>> m_gBufferImages;
		std::vector<Format> m_imageFormats;
		std::unique_ptr<IRenderImage> m_depthImage;
	};
}