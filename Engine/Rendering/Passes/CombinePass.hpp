#pragma once

#include "IRenderPass.hpp"
#include <glm/glm.hpp>
#include "../Resources/IImageView.hpp"
#include "../Resources/IRenderImage.hpp"
#include "../Types.hpp"

namespace Engine::Rendering
{
	class IResourceFactory;

	class CombinePass : public IRenderPass
	{
	public:
		CombinePass();

		virtual bool Build(const Renderer& renderer, const std::unordered_map<const char*, IRenderImage*>& imageInputs,
			const std::unordered_map<const char*, IBuffer*>& bufferInputs) override;

		virtual void Draw(const IDevice& device, const ICommandBuffer& commandBuffer, uint32_t frameIndex) const override;

		inline IRenderImage& GetOutputImage() const { return *m_outputImage; }
		inline const IImageView& GetOutputImageView() const { return m_outputImage->GetView(); }

		void SetDebugMode(uint32_t value) const;

	private:
		bool CreateOutputImage(const IDevice& device, const IResourceFactory& resourceFactory, const glm::uvec2& size);

		std::unique_ptr<IRenderImage> m_outputImage;
	};
}