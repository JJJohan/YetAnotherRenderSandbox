#include "TAAPass.hpp"
#include "../Resources/IBuffer.hpp"
#include "../Resources/IRenderImage.hpp"
#include "../IDevice.hpp"
#include "../Resources/ICommandBuffer.hpp"
#include "../IResourceFactory.hpp"
#include "../ISwapChain.hpp"
#include "../Renderer.hpp"

using namespace Engine::Logging;

namespace Engine::Rendering
{
	TAAPass::TAAPass()
		: IRenderPass("TAA", "TAA")
		, m_taaPreviousImages()
	{
		m_imageInputs =
		{
			"Combined",
			"Velocity",
			"Depth"
		};

		m_imageOutputs =
		{
			"Final"
		};
	}

	bool TAAPass::CreateTAAImage(const IDevice& device, const IResourceFactory& resourceFactory, const glm::uvec2& size)
	{
		Format format = Format::R8G8B8A8Unorm;
		for (size_t i = 0; i < m_taaPreviousImages.size(); ++i)
		{
			ImageUsageFlags usageFlags = i == 0 ?
				ImageUsageFlags::Sampled | ImageUsageFlags::TransferDst :
				ImageUsageFlags::ColorAttachment | ImageUsageFlags::TransferSrc;

			m_taaPreviousImages[i] = std::move(resourceFactory.CreateRenderImage());
			glm::uvec3 extent(size.x, size.y, 1);
			if (!m_taaPreviousImages[i]->Initialise(device, ImageType::e2D, format, extent, 1, 1,
				ImageTiling::Optimal, usageFlags, ImageAspectFlags::Color, MemoryUsage::AutoPreferDevice,
				AllocationCreateFlags::None, SharingMode::Exclusive))
			{
				Logger::Error("Failed to create image.");
				return false;
			}
		}

		return true;
	}

	void TAAPass::TransitionTAAImageLayouts(const IDevice& device, const ICommandBuffer& commandBuffer) const
	{
		m_taaPreviousImages[0]->TransitionImageLayout(device, commandBuffer, ImageLayout::ShaderReadOnly);
		m_taaPreviousImages[1]->TransitionImageLayout(device, commandBuffer, ImageLayout::ColorAttachment);
	}

	void TAAPass::BlitTAA(const IDevice& device, const ICommandBuffer& commandBuffer) const
	{
		m_taaPreviousImages[0]->TransitionImageLayout(device, commandBuffer, ImageLayout::TransferDst);
		m_taaPreviousImages[1]->TransitionImageLayout(device, commandBuffer, ImageLayout::TransferSrc);

		const glm::uvec3& extents = m_taaPreviousImages[0]->GetDimensions();
		const glm::uvec3 offset(extents.x, extents.y, extents.z);

		ImageBlit blit;
		blit.srcSubresource = ImageSubresourceLayers(ImageAspectFlags::Color, 0, 0, 1);
		blit.srcOffsets = std::array<glm::uvec3, 2> { glm::uvec3(), offset };
		blit.dstSubresource = ImageSubresourceLayers(ImageAspectFlags::Color, 0, 0, 1);
		blit.dstOffsets = std::array<glm::uvec3, 2> { glm::uvec3(), offset };

		commandBuffer.BlitImage(*m_taaPreviousImages[1], ImageLayout::TransferSrc,
			*m_taaPreviousImages[0], ImageLayout::TransferDst, { blit }, Filter::Linear);
	}

	bool TAAPass::Build(const Renderer& renderer, const std::unordered_map<const char*, IRenderImage*>& imageInputs,
		const std::unordered_map<const char*, IBuffer*>& bufferInputs)
	{
		m_taaPreviousImages[0].reset();
		m_taaPreviousImages[1].reset();
		m_imageResources.clear();

		const IDevice& device = renderer.GetDevice();
		const IResourceFactory& resourceFactory = renderer.GetResourceFactory();
		const ISwapChain& swapchain = renderer.GetSwapChain();

		const glm::uvec2& size = swapchain.GetExtent();
		if (!CreateTAAImage(device, resourceFactory, size))
		{
			return false;
		}

		m_imageResources["History"] = m_taaPreviousImages[1].get();

		const IImageSampler& linearSampler = renderer.GetLinearSampler();
		const IImageSampler& nearestSampler = renderer.GetNearestSampler();
		const IImageView& combinedImageView = imageInputs.at("Combined")->GetView();
		const IImageView& velocityImageView = imageInputs.at("Velocity")->GetView();
		const IImageView& depthImageView = imageInputs.at("Depth")->GetView();

		if (!m_material->BindSampler(0, linearSampler) ||
			!m_material->BindSampler(1, nearestSampler) ||
			!m_material->BindImageView(2, combinedImageView) ||
			!m_material->BindImageView(3, m_taaPreviousImages[0]->GetView()) ||
			!m_material->BindImageView(4, velocityImageView) ||
			!m_material->BindImageView(5, depthImageView))
			return false;

		return true;
	}

	void TAAPass::Draw(const IDevice& device, const ICommandBuffer& commandBuffer, uint32_t frameIndex) const
	{
		m_material->BindMaterial(commandBuffer, frameIndex);

		uint32_t enabled = 1; // TODO: Remove!
		commandBuffer.PushConstants(m_material, ShaderStageFlags::Vertex, 0, sizeof(uint32_t), &enabled);
		commandBuffer.Draw(3, 1, 0, 0);
	}
}