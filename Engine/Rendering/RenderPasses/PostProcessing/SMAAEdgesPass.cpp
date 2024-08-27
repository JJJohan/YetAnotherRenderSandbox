#include "SMAAEdgesPass.hpp"
#include "../../Resources/IBuffer.hpp"
#include "../../Resources/IRenderImage.hpp"
#include "../../Resources/ICommandBuffer.hpp"
#include "../../Renderer.hpp"

namespace Engine::Rendering
{
	SMAAEdgesPass::SMAAEdgesPass()
		: IRenderPass("SMAAEdges", "SMAAEdges")
	{
		m_imageInputInfos =
		{
			{"Output", RenderPassImageInfo(AccessFlags::Read, Format::PlaceholderSwapchain, {}, ImageLayout::ShaderReadOnly,
				MaterialStageFlags::FragmentShader, MaterialAccessFlags::ShaderRead)}
		};

		m_imageOutputInfos =
		{
			{"Edges", RenderPassImageInfo(AccessFlags::Write, Format::R8G8Unorm, {}, ImageLayout::ColorAttachment,
				MaterialStageFlags::ColorAttachmentOutput, MaterialAccessFlags::ColorAttachmentRead | MaterialAccessFlags::ColorAttachmentWrite)}
		};
	}

	void SMAAEdgesPass::UpdatePlaceholderFormats(Format swapchainFormat, Format depthFormat)
	{
		m_imageInputInfos.at("Output").Format = swapchainFormat;
	}

	bool SMAAEdgesPass::Build(const Renderer& renderer,
		const std::unordered_map<std::string, IRenderImage*>& imageInputs,
		const std::unordered_map<std::string, IRenderImage*>& imageOutputs,
		const std::unordered_map<std::string, IBuffer*>& bufferInputs,
		const std::unordered_map<std::string, IBuffer*>& bufferOutputs)
	{
		ClearResources();

		m_colourAttachments.emplace_back(m_material->GetColourAttachmentInfo(0, imageOutputs.at("Edges"), AttachmentLoadOp::Clear));

		const std::vector<std::unique_ptr<IBuffer>>& frameInfoBuffers = renderer.GetFrameInfoBuffers();

		const IImageSampler& linearSampler = renderer.GetLinearSampler();
		const IImageView& outputImageView = imageInputs.at("Output")->GetView();

		if (!m_material->BindUniformBuffers(0, frameInfoBuffers) ||
			!m_material->BindCombinedImageSampler(1, linearSampler, outputImageView, ImageLayout::ShaderReadOnly))
			return false;

		return IRenderNode::Build(renderer, imageInputs, imageOutputs, bufferInputs, bufferOutputs);
	}

	void SMAAEdgesPass::Draw(const Renderer& renderer, const ICommandBuffer& commandBuffer,
		const glm::uvec2& size, uint32_t frameIndex, uint32_t layerIndex)
	{
		m_material->BindMaterial(commandBuffer, BindPoint::Graphics, frameIndex);
		commandBuffer.Draw(3, 1, 0, 0);
	}
}