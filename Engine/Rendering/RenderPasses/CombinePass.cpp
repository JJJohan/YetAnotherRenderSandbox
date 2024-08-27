#include "CombinePass.hpp"
#include "../Resources/IBuffer.hpp"
#include "../Resources/IRenderImage.hpp"
#include "../Resources/ICommandBuffer.hpp"
#include "../Renderer.hpp"
#include "../RenderResources/ShadowMap.hpp"

namespace Engine::Rendering
{
	CombinePass::CombinePass(const ShadowMap& shadowMap)
		: IRenderPass("Combine", "Combine")
		, m_shadowMap(shadowMap)
	{
		m_imageInputInfos =
		{
			{"Albedo", RenderPassImageInfo(AccessFlags::Read, Format::R8G8B8A8Unorm, {}, ImageLayout::ShaderReadOnly,
				MaterialStageFlags::FragmentShader, MaterialAccessFlags::ShaderRead)},
			{"WorldNormal", RenderPassImageInfo(AccessFlags::Read, Format::R16G16B16A16Sfloat, {}, ImageLayout::ShaderReadOnly,
				MaterialStageFlags::FragmentShader, MaterialAccessFlags::ShaderRead)},
			{"WorldPos", RenderPassImageInfo(AccessFlags::Read, Format::R16G16B16A16Sfloat, {}, ImageLayout::ShaderReadOnly,
				MaterialStageFlags::FragmentShader, MaterialAccessFlags::ShaderRead)},
			{"MetalRoughness", RenderPassImageInfo(AccessFlags::Read, Format::R8G8Unorm, {}, ImageLayout::ShaderReadOnly,
				MaterialStageFlags::FragmentShader, MaterialAccessFlags::ShaderRead)},
			{"Shadows", RenderPassImageInfo(AccessFlags::Read, Format::PlaceholderDepth, shadowMap.GetExtent(), ImageLayout::ShaderReadOnly,
				MaterialStageFlags::FragmentShader, MaterialAccessFlags::ShaderRead)}
		};

		m_imageOutputInfos =
		{
			{"Output", RenderPassImageInfo(AccessFlags::Write, Format::R16G16B16A16Sfloat, {}, ImageLayout::ColorAttachment,
				MaterialStageFlags::ColorAttachmentOutput, MaterialAccessFlags::ColorAttachmentRead | MaterialAccessFlags::ColorAttachmentWrite)}
		};
	}

	void CombinePass::UpdatePlaceholderFormats(Format swapchainFormat, Format depthFormat)
	{
		auto& inputShadowImage = m_imageInputInfos.at("Shadows");
		inputShadowImage.Format = depthFormat;
		inputShadowImage.Dimensions = m_shadowMap.GetExtent();
	}

	bool CombinePass::Build(const Renderer& renderer,
		const std::unordered_map<std::string, IRenderImage*>& imageInputs,
		const std::unordered_map<std::string, IRenderImage*>& imageOutputs,
		const std::unordered_map<std::string, IBuffer*>& bufferInputs,
		const std::unordered_map<std::string, IBuffer*>& bufferOutputs)
	{
		ClearResources();

		m_colourAttachments.emplace_back(m_material->GetColourAttachmentInfo(0, imageOutputs.at("Output")));

		const std::vector<std::unique_ptr<IBuffer>>& frameInfoBuffers = renderer.GetFrameInfoBuffers();
		const std::vector<std::unique_ptr<IBuffer>>& lightBuffers = renderer.GetLightBuffers();
		const IImageSampler& linearSampler = renderer.GetLinearSampler();
		const IImageSampler& shadowSampler = renderer.GetShadowSampler();

		std::vector<const IImageView*> imageViews =
		{
			&imageInputs.at("Albedo")->GetView(),
			&imageInputs.at("WorldNormal")->GetView(),
			&imageInputs.at("WorldPos")->GetView(),
			&imageInputs.at("MetalRoughness")->GetView()
		};

		const IImageView& shadowImageView = imageInputs.at("Shadows")->GetView();

		if (!m_material->BindUniformBuffers(0, frameInfoBuffers) ||
			!m_material->BindUniformBuffers(1, lightBuffers) ||
			!m_material->BindSampler(2, linearSampler) ||
			!m_material->BindImageViews(3, imageViews) ||
			!m_material->BindSampler(4, shadowSampler) ||
			!m_material->BindImageView(5, shadowImageView))
			return false;

		return IRenderNode::Build(renderer, imageInputs, imageOutputs, bufferInputs, bufferOutputs);
	}

	void CombinePass::Draw(const Renderer& renderer, const ICommandBuffer& commandBuffer,
		const glm::uvec2& size, uint32_t frameIndex, uint32_t layerIndex)
	{
		m_material->BindMaterial(commandBuffer, BindPoint::Graphics, frameIndex);
		commandBuffer.Draw(3, 1, 0, 0);
	}
}