#include "UIPass.hpp"
#include "../Resources/IBuffer.hpp"
#include "../Resources/IRenderImage.hpp"
#include "../Resources/ICommandBuffer.hpp"
#include "../Renderer.hpp"
#include "UI/UIManager.hpp"

using namespace Engine::UI;

namespace Engine::Rendering
{
	UIPass::UIPass(UIManager& uiManager)
		: IRenderPass("UI", "")
		, m_uiManager(uiManager)
	{
		m_imageInputInfos =
		{
			{"Output", RenderPassImageInfo(AccessFlags::None, Format::PlaceholderSwapchain)}
		};

		m_imageOutputInfos =
		{
			{"Output", RenderPassImageInfo(AccessFlags::Write, Format::PlaceholderSwapchain, {}, ImageLayout::ColorAttachment,
				MaterialStageFlags::ColorAttachmentOutput, MaterialAccessFlags::ColorAttachmentRead | MaterialAccessFlags::ColorAttachmentWrite)}
		};
	}

	void UIPass::UpdatePlaceholderFormats(Format swapchainFormat, Format depthFormat)
	{
		m_imageInputInfos.at("Output").Format = swapchainFormat;
		m_imageOutputInfos.at("Output").Format = swapchainFormat;
	}

	bool UIPass::Build(const Renderer& renderer,
		const std::unordered_map<std::string, IRenderImage*>& imageInputs,
		const std::unordered_map<std::string, IRenderImage*>& imageOutputs,
		const std::unordered_map<std::string, IBuffer*>& bufferInputs,
		const std::unordered_map<std::string, IBuffer*>& bufferOutputs)
	{
		ClearResources();

		m_colourAttachments.emplace_back(AttachmentInfo(imageOutputs.at("Output"), ImageLayout::ColorAttachment, AttachmentLoadOp::Load));

		return IRenderNode::Build(renderer, imageInputs, imageOutputs, bufferInputs, bufferOutputs);
	}

	void UIPass::Draw(const Renderer& renderer, const ICommandBuffer& commandBuffer,
		const glm::uvec2& size, uint32_t frameIndex, uint32_t layerIndex)
	{
		m_uiManager.Draw(commandBuffer, static_cast<float>(size.x), static_cast<float>(size.y));
	}
}