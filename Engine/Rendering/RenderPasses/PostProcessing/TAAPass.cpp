#include "TAAPass.hpp"
#include "../../Resources/IBuffer.hpp"
#include "../../Resources/IRenderImage.hpp"
#include "../../IDevice.hpp"
#include "../../Resources/ICommandBuffer.hpp"
#include "../../Resources/IMemoryBarriers.hpp"
#include "../../IResourceFactory.hpp"
#include "../../ISwapChain.hpp"
#include "../../Renderer.hpp"

namespace Engine::Rendering
{
	TAAPass::TAAPass()
		: IRenderPass("TAA", "TAA")
		, m_taaHistoryImage()
	{
		m_imageInputInfos =
		{
			{"Output", RenderPassImageInfo(AccessFlags::Read, Format::PlaceholderSwapchain, {}, ImageLayout::ShaderReadOnly,
				MaterialStageFlags::FragmentShader, MaterialAccessFlags::ShaderRead)},
			{"Velocity", RenderPassImageInfo(AccessFlags::Read, Format::R16G16Sfloat, {}, ImageLayout::ShaderReadOnly,
				MaterialStageFlags::FragmentShader, MaterialAccessFlags::ShaderRead)},
			{"Depth", RenderPassImageInfo(AccessFlags::Read, Format::D32Sfloat, {}, ImageLayout::ShaderReadOnly,
				MaterialStageFlags::FragmentShader, MaterialAccessFlags::ShaderRead)}
		};

		m_imageOutputInfos =
		{
			{"Output", RenderPassImageInfo(AccessFlags::Write, Format::PlaceholderSwapchain, {}, ImageLayout::ColorAttachment,
				MaterialStageFlags::ColorAttachmentOutput, MaterialAccessFlags::ColorAttachmentRead | MaterialAccessFlags::ColorAttachmentWrite)}
		};
	}

	void TAAPass::UpdatePlaceholderFormats(Format swapchainFormat, Format depthFormat)
	{
		m_imageInputInfos.at("Output").Format = swapchainFormat;
		m_imageOutputInfos.at("Output").Format = swapchainFormat;
	}

	bool TAAPass::CreateTAAHistoryImage(const IDevice& device, const IResourceFactory& resourceFactory, const glm::uvec2& size)
	{
		Format format = Format::R8G8B8A8Unorm;

		ImageUsageFlags usageFlags = ImageUsageFlags::Sampled | ImageUsageFlags::TransferDst;
		m_taaHistoryImage = std::move(resourceFactory.CreateRenderImage());
		glm::uvec3 extent(size.x, size.y, 1);
		if (!m_taaHistoryImage->Initialise("TAAHistoryImage", device, ImageType::e2D, format, extent, 1, 1,
			ImageTiling::Optimal, usageFlags, ImageAspectFlags::Color, MemoryUsage::AutoPreferDevice,
			AllocationCreateFlags::None, SharingMode::Exclusive))
		{
			Logger::Error("Failed to create TAA history image.");
			return false;
		}

		return true;
	}

	bool TAAPass::Build(const Renderer& renderer,
		const std::unordered_map<std::string, IRenderImage*>& imageInputs,
		const std::unordered_map<std::string, IRenderImage*>& imageOutputs,
		const std::unordered_map<std::string, IBuffer*>& bufferInputs,
		const std::unordered_map<std::string, IBuffer*>& bufferOutputs)
	{
		ClearResources();

		const IDevice& device = renderer.GetDevice();
		const IResourceFactory& resourceFactory = renderer.GetResourceFactory();
		const ISwapChain& swapchain = renderer.GetSwapChain();

		const glm::uvec2& size = swapchain.GetExtent();
		if (!CreateTAAHistoryImage(device, resourceFactory, size))
		{
			return false;
		}

		m_colourAttachments.emplace_back(m_material->GetColourAttachmentInfo(0, imageOutputs.at("Output")));

		const IImageSampler& linearSampler = renderer.GetLinearSampler();
		const IImageSampler& nearestSampler = renderer.GetNearestSampler();
		const IImageView& outputImageView = imageInputs.at("Output")->GetView();
		const IImageView& velocityImageView = imageInputs.at("Velocity")->GetView();
		const IImageView& depthImageView = imageInputs.at("Depth")->GetView();

		if (!m_material->BindSampler(0, linearSampler) ||
			!m_material->BindSampler(1, nearestSampler) ||
			!m_material->BindImageView(2, outputImageView) ||
			!m_material->BindImageView(3, m_taaHistoryImage->GetView()) ||
			!m_material->BindImageView(4, velocityImageView) ||
			!m_material->BindImageView(5, depthImageView))
			return false;

		return IRenderNode::Build(renderer, imageInputs, imageOutputs, bufferInputs, bufferOutputs);
	}

	void TAAPass::PreDraw(const Renderer& renderer, const ICommandBuffer& commandBuffer,
		const glm::uvec2& size, uint32_t frameIndex, const std::unordered_map<std::string, IRenderImage*>& imageInputs,
		const std::unordered_map<std::string, IRenderImage*>& imageOutputs)
	{
		const IDevice& device = renderer.GetDevice();

		std::unique_ptr<IMemoryBarriers> memoryBarriers = std::move(renderer.GetResourceFactory().CreateMemoryBarriers());

		if (m_taaHistoryImage->GetLayout() == ImageLayout::Undefined)
		{
			m_taaHistoryImage->AppendImageLayoutTransitionExt(commandBuffer,
				MaterialStageFlags::Transfer, ImageLayout::TransferDst, MaterialAccessFlags::TransferWrite, *memoryBarriers);
			commandBuffer.MemoryBarrier(*memoryBarriers);
			memoryBarriers->Clear();

			commandBuffer.ClearColourImage(*m_taaHistoryImage, Engine::Colour());
		}

		m_taaHistoryImage->AppendImageLayoutTransition(commandBuffer, ImageLayout::ShaderReadOnly, *memoryBarriers);
		commandBuffer.MemoryBarrier(*memoryBarriers);
	}

	void TAAPass::Draw(const Renderer& renderer, const ICommandBuffer& commandBuffer,
		const glm::uvec2& size, uint32_t frameIndex, uint32_t layerIndex)
	{
		m_material->BindMaterial(commandBuffer, BindPoint::Graphics, frameIndex);
		commandBuffer.Draw(3, 1, 0, 0);
	}

	void TAAPass::PostDraw(const Renderer& renderer, const ICommandBuffer& commandBuffer,
		const glm::uvec2& size, uint32_t frameIndex, const std::unordered_map<std::string, IRenderImage*>& imageInputs,
		const std::unordered_map<std::string, IRenderImage*>& imageOutputs)
	{
		const IDevice& device = renderer.GetDevice();
		IRenderImage* outputImage = imageOutputs.at("Output");

		std::unique_ptr<IMemoryBarriers> memoryBarriers = std::move(renderer.GetResourceFactory().CreateMemoryBarriers());
		m_taaHistoryImage->AppendImageLayoutTransition(commandBuffer, ImageLayout::TransferDst, *memoryBarriers);
		outputImage->AppendImageLayoutTransition(commandBuffer, ImageLayout::TransferSrc, *memoryBarriers);
		commandBuffer.MemoryBarrier(*memoryBarriers);

		const glm::uvec3& extents = m_taaHistoryImage->GetDimensions();
		const glm::uvec3 offset(extents.x, extents.y, extents.z);

		ImageBlit blit;
		blit.srcSubresource = ImageSubresourceLayers(ImageAspectFlags::Color, 0, 0, 1);
		blit.srcOffsets = std::array<glm::uvec3, 2>{ glm::uvec3(), offset };
		blit.dstSubresource = ImageSubresourceLayers(ImageAspectFlags::Color, 0, 0, 1);
		blit.dstOffsets = std::array<glm::uvec3, 2>{ glm::uvec3(), offset };

		commandBuffer.BlitImage(*outputImage, *m_taaHistoryImage, { blit }, Filter::Linear);
	}
}