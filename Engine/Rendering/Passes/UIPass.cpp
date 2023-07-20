#include "UIPass.hpp"
#include "../Resources/IBuffer.hpp"
#include "../Resources/IRenderImage.hpp"
#include "../IDevice.hpp"
#include "../Resources/ICommandBuffer.hpp"
#include "../IResourceFactory.hpp"
#include "../ISwapChain.hpp"
#include "../Renderer.hpp"
#include "UI/UIManager.hpp"

using namespace Engine::Logging;
using namespace Engine::UI;

namespace Engine::Rendering
{
	const Format OutputImageFormat = Format::R8G8B8A8Unorm;

	UIPass::UIPass(UIManager& uiManager)
		: IRenderPass("UI", nullptr)
		, m_uiManager(uiManager)
	{
		m_imageInputInfos =
		{
			{"Output", RenderPassImageInfo(OutputImageFormat)}
		};

		m_imageOutputInfos =
		{
			{"Output", RenderPassImageInfo(OutputImageFormat)}
		};
	}

	bool UIPass::Build(const Renderer& renderer,
		const std::unordered_map<const char*, IRenderImage*>& imageInputs,
		const std::unordered_map<const char*, IRenderImage*>& imageOutputs)
	{
		ClearResources();

		m_colourAttachments.emplace_back(AttachmentInfo(imageOutputs.at("Output"), ImageLayout::ColorAttachment, AttachmentLoadOp::Load));

		return true;
	}

	void UIPass::Draw(const IDevice& device, const ICommandBuffer& commandBuffer,
		const glm::uvec2& size, uint32_t frameIndex, uint32_t layerIndex)
	{
		m_uiManager.Draw(commandBuffer, static_cast<float>(size.x), static_cast<float>(size.y));
	}
}