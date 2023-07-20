#include "CombinePass.hpp"
#include "Core/Logging/Logger.hpp"
#include "../Resources/IBuffer.hpp"
#include "../Resources/IRenderImage.hpp"
#include "../IDevice.hpp"
#include "../Resources/ICommandBuffer.hpp"
#include "../IResourceFactory.hpp"
#include "../Renderer.hpp"
#include "../RenderResources/ShadowMap.hpp"

using namespace Engine::Logging;

namespace Engine::Rendering
{
	const Format OutputImageFormat = Format::R8G8B8A8Unorm;

	CombinePass::CombinePass(const ShadowMap& shadowMap)
		: IRenderPass("Combine", "Combine")
		, m_shadowMap(shadowMap)
	{
		m_imageInputInfos =
		{
			{"Albedo", RenderPassImageInfo(Format::R8G8B8A8Unorm, true)},
			{"WorldNormal", RenderPassImageInfo(Format::R16G16B16A16Sfloat, true)},
			{"WorldPos", RenderPassImageInfo(Format::R16G16B16A16Sfloat, true)},
			{"MetalRoughness", RenderPassImageInfo(Format::R8G8Unorm, true)},
			{"Shadows", RenderPassImageInfo(Format::PlaceholderDepth, true, shadowMap.GetExtent())}
		};

		m_imageOutputInfos = 
		{
			{"Output", RenderPassImageInfo(OutputImageFormat)}
		};
	}

	bool CombinePass::Build(const Renderer& renderer,
		const std::unordered_map<const char*, IRenderImage*>& imageInputs,
		const std::unordered_map<const char*, IRenderImage*>& imageOutputs)
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

		return true;
	}

	void CombinePass::Draw(const IDevice& device, const ICommandBuffer& commandBuffer,
		const glm::uvec2& size, uint32_t frameIndex, uint32_t layerIndex)
	{
		m_material->BindMaterial(commandBuffer, frameIndex);
		commandBuffer.Draw(3, 1, 0, 0);
	}

	void CombinePass::SetDebugMode(uint32_t value) const
	{
		m_material->SetSpecialisationConstant("debugMode", static_cast<int32_t>(value));
	}
}