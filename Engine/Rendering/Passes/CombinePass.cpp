#include "CombinePass.hpp"
#include "Core/Logging/Logger.hpp"
#include "../Resources/IBuffer.hpp"
#include "../Resources/IRenderImage.hpp"
#include "../IDevice.hpp"
#include "../Resources/ICommandBuffer.hpp"
#include "../IResourceFactory.hpp"
#include "../Renderer.hpp"

using namespace Engine::Logging;

namespace Engine::Rendering
{
	const Format OutputImageFormat = Format::R8G8B8A8Unorm;

	CombinePass::CombinePass()
		: IRenderPass("Combine", "Combine")
		, m_outputImage()
	{
		m_imageInputs =
		{
			"Albedo",
			"WorldNormal",
			"WorldPos",
			"MetalRoughness",
			"Shadows"
		};

		m_imageOutputs = 
		{
			"Combined"
		};
	}

	bool CombinePass::CreateOutputImage(const IDevice& device, const IResourceFactory& resourceFactory, const glm::uvec2& size)
	{
		m_outputImage = std::move(resourceFactory.CreateRenderImage());
		glm::uvec3 extent(size.x, size.y, 1);
		if (!m_outputImage->Initialise(device, ImageType::e2D, OutputImageFormat, extent, 1, 1,
			ImageTiling::Optimal, ImageUsageFlags::ColorAttachment | ImageUsageFlags::Sampled,
			ImageAspectFlags::Color, MemoryUsage::AutoPreferDevice, AllocationCreateFlags::None, SharingMode::Exclusive))
		{
			Logger::Error("Failed to create image.");
			return false;
		}

		return true;
	}

	bool CombinePass::Build(const Renderer& renderer, const std::unordered_map<const char*, IRenderImage*>& imageInputs,
		const std::unordered_map<const char*, IBuffer*>& bufferInputs)
	{
		m_imageResources.clear();
		m_outputImage.reset();

		const IDevice& device = renderer.GetDevice();
		const IResourceFactory& resourceFactory = renderer.GetResourceFactory();
		const ISwapChain& swapchain = renderer.GetSwapChain();

		const glm::uvec2& size = swapchain.GetExtent();
		if (!CreateOutputImage(device, resourceFactory, size))
		{
			return false;
		}

		m_imageResources["Combined"] = m_outputImage.get();

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

	void CombinePass::Draw(const IDevice& device, const ICommandBuffer& commandBuffer, uint32_t frameIndex) const
	{
		m_material->BindMaterial(commandBuffer, frameIndex);
		commandBuffer.Draw(3, 1, 0, 0);
	}

	void CombinePass::SetDebugMode(uint32_t value) const
	{
		m_material->SetSpecialisationConstant("debugMode", static_cast<int32_t>(value));
	}
}