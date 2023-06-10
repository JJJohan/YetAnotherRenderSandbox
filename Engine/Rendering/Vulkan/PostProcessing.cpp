#include "PostProcessing.hpp"
#include "Core/Logging/Logger.hpp"
#include "RenderImage.hpp"
#include "ImageView.hpp"
#include "ImageSampler.hpp"
#include "Device.hpp"
#include "GBuffer.hpp"
#include "PhysicalDevice.hpp"
#include "PipelineLayout.hpp"
#include "PipelineManager.hpp"
#include "OS/Files.hpp"

using namespace Engine::Logging;
using namespace Engine::OS;

namespace Engine::Rendering::Vulkan
{
	PostProcessing::PostProcessing(GBuffer& gBuffer)
		: m_linearSampler()
		, m_nearestSampler()
		, m_enabled(true)
		, m_taaJitterOffsets()
		, m_taaPreviousImages()
		, m_taaPreviousImageViews()
		, m_taaFrameIndex(0)
		, m_gBuffer(gBuffer)
		, m_taaShader(nullptr)
	{
	}

	bool PostProcessing::Initialise(const PhysicalDevice& physicalDevice, const Device& device,
		const PipelineManager& pipelineManager, VmaAllocator allocator, const glm::uvec2& size)
	{
		m_linearSampler = std::make_unique<ImageSampler>();
		if (!m_linearSampler->Initialise(device, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear,
			vk::SamplerAddressMode::eRepeat, 1))
		{
			return false;
		}

		m_nearestSampler = std::make_unique<ImageSampler>();
		if (!m_nearestSampler->Initialise(device, vk::Filter::eNearest, vk::Filter::eNearest, vk::SamplerMipmapMode::eNearest,
			vk::SamplerAddressMode::eRepeat, 1))
		{
			return false;
		}

		if (!pipelineManager.TryGetPipelineLayout("TAA", &m_taaShader))
		{
			return false;
		}

		if (!Rebuild(physicalDevice, device, allocator, size))
		{
			return false;
		}

		return true;
	}

	bool PostProcessing::Rebuild(const PhysicalDevice& physicalDevice, const Device& device,
		VmaAllocator allocator, const glm::uvec2& size)
	{
		if (!RebuildTAA(physicalDevice, device, allocator, m_gBuffer.GetOutputImageView(), size))
		{
			return false;
		}

		return true;
	}

	float Halton(size_t i, uint32_t b)
	{
		float f = 1.0f;
		float r = 0.0f;

		while (i > 0)
		{
			f /= static_cast<float>(b);
			r = r + f * static_cast<float>(i % b);
			i = static_cast<uint32_t>(floorf(static_cast<float>(i) / static_cast<float>(b)));
		}

		return r;
	}

	bool PostProcessing::RebuildTAA(const PhysicalDevice& physicalDevice, const Device& device,
		VmaAllocator allocator, const ImageView& inputImageView, const glm::uvec2& size)
	{
		m_taaPreviousImages[0].reset();
		m_taaPreviousImages[1].reset();
		m_taaPreviousImageViews[0].reset();
		m_taaPreviousImageViews[1].reset();

		// Populate TAA jitter offsets with quasi-random sequence.
		for (size_t i = 0; i < m_taaJitterOffsets.size(); ++i)
		{
			m_taaJitterOffsets[i].x = (2.0f * Halton(i + 1, 2) - 1.0f) / size.x;
			m_taaJitterOffsets[i].y = (2.0f * Halton(i + 1, 3) - 1.0f) / size.y;
		}

		if (!CreateTAAImage(device, allocator, size))
		{
			return false;
		}

		if (!m_taaShader->BindSampler(0, m_linearSampler) ||
			!m_taaShader->BindSampler(1, m_nearestSampler) ||
			!m_taaShader->BindImageView(2, inputImageView) ||
			!m_taaShader->BindImageView(3, m_taaPreviousImageViews[0]) ||
			!m_taaShader->BindImageView(4, m_gBuffer.GetVelocityImageView()) ||
			!m_taaShader->BindImageView(5, m_gBuffer.GetDepthImageView()))
			return false;

		return true;
	}

	bool PostProcessing::CreateTAAImage(const Device& device, VmaAllocator allocator, const glm::uvec2& size)
	{
		vk::Format format = vk::Format::eR8G8B8A8Unorm;
		for (size_t i = 0; i < m_taaPreviousImages.size(); ++i)
		{
			vk::ImageUsageFlags usageFlags = i == 0 ?
				vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst :
				vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc;

			m_taaPreviousImages[i] = std::make_unique<RenderImage>(allocator);
			vk::Extent3D extent(size.x, size.y, 1);
			if (!m_taaPreviousImages[i]->Initialise(vk::ImageType::e2D, format, extent, 1, vk::ImageTiling::eOptimal, usageFlags,
				VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0, vk::SharingMode::eExclusive))
			{
				Logger::Error("Failed to create image.");
				return false;
			}

			m_taaPreviousImageViews[i] = std::make_unique<ImageView>();
			if (!m_taaPreviousImageViews[i]->Initialise(device, m_taaPreviousImages[i]->Get(), 1, format, vk::ImageAspectFlagBits::eColor))
			{
				Logger::Error("Failed to create image view.");
				return false;
			}
		}

		return true;
	}

	void PostProcessing::TransitionTAAImageLayouts(const Device& device, const vk::CommandBuffer& commandBuffer) const
	{
		m_taaPreviousImages[0]->TransitionImageLayout(device, commandBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
		m_taaPreviousImages[1]->TransitionImageLayout(device, commandBuffer, vk::ImageLayout::eColorAttachmentOptimal);
	}

	void PostProcessing::BlitTAA(const Device& device, const vk::CommandBuffer& commandBuffer) const
	{
		m_taaPreviousImages[0]->TransitionImageLayout(device, commandBuffer, vk::ImageLayout::eTransferDstOptimal);
		m_taaPreviousImages[1]->TransitionImageLayout(device, commandBuffer, vk::ImageLayout::eTransferSrcOptimal);

		const vk::Extent3D& extents = m_taaPreviousImages[0]->GetDimensions();
		vk::Offset3D offset(extents.width, extents.height, extents.depth);

		vk::ImageBlit blit;
		blit.srcSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);
		blit.srcOffsets = std::array<vk::Offset3D, 2> { vk::Offset3D(), offset };
		blit.dstSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);
		blit.dstOffsets = std::array<vk::Offset3D, 2> { vk::Offset3D(), offset };

		commandBuffer.blitImage(m_taaPreviousImages[1]->Get(), vk::ImageLayout::eTransferSrcOptimal,
			m_taaPreviousImages[0]->Get(), vk::ImageLayout::eTransferDstOptimal, { blit }, vk::Filter::eLinear);
	}

	void PostProcessing::Draw(const vk::CommandBuffer& commandBuffer, uint32_t frameIndex) const
	{
		m_taaShader->BindPipeline(commandBuffer, frameIndex);

		uint32_t enabled = m_enabled ? 1 : 0;
		commandBuffer.pushConstants(m_taaShader->Get(), vk::ShaderStageFlagBits::eVertex, 0, sizeof(uint32_t), &enabled);
		commandBuffer.draw(3, 1, 0, 0);
	}
}