#include "SMAABlendPass.hpp"
#include "../../Resources/IBuffer.hpp"
#include "../../Resources/IRenderImage.hpp"
#include "../../IDevice.hpp"
#include "../../Resources/ICommandBuffer.hpp"
#include "../../Renderer.hpp"

namespace Engine::Rendering
{
	SMAABlendPass::SMAABlendPass()
		: IRenderPass("SMAABlend", "SMAABlend")
	{
		m_imageInputInfos =
		{
			{"Output", RenderPassImageInfo(Format::PlaceholderSwapchain, true)},
			{"BlendedWeights", RenderPassImageInfo(Format::R8G8B8A8Unorm, true)},
		};

		m_imageOutputInfos =
		{
			{"Output", RenderPassImageInfo(Format::PlaceholderSwapchain)}
		};
	}

	void SMAABlendPass::UpdatePlaceholderFormats(Format swapchainFormat, Format depthFormat)
	{
		m_imageInputInfos.at("Output").Format = swapchainFormat;
		m_imageOutputInfos.at("Output").Format = swapchainFormat;
	}

	bool SMAABlendPass::Build(const Renderer& renderer,
		const std::unordered_map<std::string, IRenderImage*>& imageInputs,
		const std::unordered_map<std::string, IRenderImage*>& imageOutputs,
		const std::unordered_map<std::string, IBuffer*>& bufferInputs,
		const std::unordered_map<std::string, IBuffer*>& bufferOutputs)
	{
		ClearResources();

		m_colourAttachments.emplace_back(m_material->GetColourAttachmentInfo(0, imageOutputs.at("Output")));

		const std::vector<std::unique_ptr<IBuffer>>& frameInfoBuffers = renderer.GetFrameInfoBuffers();

		const IImageSampler& linearSampler = renderer.GetLinearSampler();
		const IImageView& outputImageView = imageInputs.at("Output")->GetView();
		const IImageView& blendedWeightsImageView = imageInputs.at("BlendedWeights")->GetView();

		if (!m_material->BindUniformBuffers(0, frameInfoBuffers) ||
			!m_material->BindCombinedImageSampler(1, linearSampler, outputImageView, ImageLayout::ShaderReadOnly) ||
			!m_material->BindCombinedImageSampler(2, linearSampler, blendedWeightsImageView, ImageLayout::ShaderReadOnly))
			return false;

		return true;
	}

	void SMAABlendPass::Draw(const Renderer& renderer, const ICommandBuffer& commandBuffer,
		const glm::uvec2& size, uint32_t frameIndex, uint32_t layerIndex)
	{
		m_material->BindMaterial(commandBuffer, BindPoint::Graphics, frameIndex);
		commandBuffer.Draw(3, 1, 0, 0);
	}
}