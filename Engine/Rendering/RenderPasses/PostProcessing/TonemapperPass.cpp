#include "TonemapperPass.hpp"
#include "../../Resources/IBuffer.hpp"
#include "../../Resources/IRenderImage.hpp"
#include "../../IDevice.hpp"
#include "../../Resources/ICommandBuffer.hpp"
#include "../../Renderer.hpp"

namespace Engine::Rendering
{
	TonemapperPass::TonemapperPass()
		: IRenderPass("Tonemapper", "Tonemapper")
	{
		m_imageInputInfos =
		{
			{"Output", RenderPassImageInfo(AccessFlags::Read, Format::PlaceholderSwapchain, {}, ImageLayout::ShaderReadOnly,
				MaterialStageFlags::FragmentShader, MaterialAccessFlags::ShaderRead)}
		};

		m_imageOutputInfos =
		{
			{"Output", RenderPassImageInfo(AccessFlags::Write, Format::PlaceholderSwapchain, {}, ImageLayout::ColorAttachment,
				MaterialStageFlags::ColorAttachmentOutput, MaterialAccessFlags::ColorAttachmentRead | MaterialAccessFlags::ColorAttachmentWrite)}
		};
	}

	void TonemapperPass::UpdatePlaceholderFormats(Format swapchainFormat, Format depthFormat)
	{
		m_imageInputInfos.at("Output").Format = swapchainFormat;
		m_imageOutputInfos.at("Output").Format = swapchainFormat;
	}

	bool TonemapperPass::Build(const Renderer& renderer,
		const std::unordered_map<std::string, IRenderImage*>& imageInputs,
		const std::unordered_map<std::string, IRenderImage*>& imageOutputs,
		const std::unordered_map<std::string, IBuffer*>& bufferInputs,
		const std::unordered_map<std::string, IBuffer*>& bufferOutputs)
	{
		ClearResources();

		bool isHdr = renderer.GetHDRState();
		m_material->SetSpecialisationConstant("isHdr", isHdr ? 1 : 0);

		const IDevice& device = renderer.GetDevice();

		m_colourAttachments.emplace_back(m_material->GetColourAttachmentInfo(0, imageOutputs.at("Output")));

		const IImageSampler& nearestSampler = renderer.GetNearestSampler();
		const IImageView& outputImageView = imageInputs.at("Output")->GetView();

		if (!m_material->BindSampler(0, nearestSampler) ||
			!m_material->BindImageView(1, outputImageView))
			return false;

		return IRenderNode::Build(renderer, imageInputs, imageOutputs, bufferInputs, bufferOutputs);
	}

	void TonemapperPass::Draw(const Renderer& renderer, const ICommandBuffer& commandBuffer,
		const glm::uvec2& size, uint32_t frameIndex, uint32_t layerIndex)
	{
		m_material->BindMaterial(commandBuffer, BindPoint::Graphics, frameIndex);
		commandBuffer.Draw(3, 1, 0, 0);
	}
}