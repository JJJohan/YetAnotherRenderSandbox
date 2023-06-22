#include "PostProcessing.hpp"
#include "Core/Logging/Logger.hpp"
#include "Resources/IImageSampler.hpp"
#include "Resources/IPhysicalDevice.hpp"
#include "Resources/IDevice.hpp"
#include "Resources/IResourceFactory.hpp"
#include "Resources/IMaterialManager.hpp"
#include "Resources/ICommandBuffer.hpp"
#include "Resources/Material.hpp"
#include "GBuffer.hpp"
#include "OS/Files.hpp"

using namespace Engine::Logging;
using namespace Engine::OS;

namespace Engine::Rendering
{
	PostProcessing::PostProcessing(GBuffer& gBuffer)
		: m_linearSampler()
		, m_nearestSampler()
		, m_enabled(true)
		, m_taaJitterOffsets()
		, m_taaPreviousImages()
		, m_taaFrameIndex(0)
		, m_gBuffer(gBuffer)
		, m_taaMaterial(nullptr)
	{
	}

	bool PostProcessing::Initialise(const IPhysicalDevice& physicalDevice, const IDevice& device,
		const IMaterialManager& materialManager, const IResourceFactory& resourceFactory, const glm::uvec2& size)
	{
		m_linearSampler = std::move(resourceFactory.CreateImageSampler());
		if (!m_linearSampler->Initialise(device, Filter::Linear, Filter::Linear, SamplerMipmapMode::Linear,
			SamplerAddressMode::Repeat, 1))
		{
			return false;
		}

		m_nearestSampler = std::move(resourceFactory.CreateImageSampler());
		if (!m_nearestSampler->Initialise(device, Filter::Nearest, Filter::Nearest, SamplerMipmapMode::Nearest,
			SamplerAddressMode::Repeat, 1))
		{
			return false;
		}

		if (!materialManager.TryGetMaterial("TAA", &m_taaMaterial))
		{
			return false;
		}

		if (!Rebuild(physicalDevice, device, resourceFactory, size))
		{
			return false;
		}

		return true;
	}

	bool PostProcessing::Rebuild(const IPhysicalDevice& physicalDevice, const IDevice& device,
		const IResourceFactory& resourceFactory, const glm::uvec2& size)
	{
		if (!RebuildTAA(physicalDevice, device, resourceFactory, m_gBuffer.GetOutputImageView(), size))
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

	bool PostProcessing::RebuildTAA(const IPhysicalDevice& physicalDevice, const IDevice& device,
		const IResourceFactory& resourceFactory, const IImageView& inputImageView, const glm::uvec2& size)
	{
		m_taaPreviousImages[0].reset();
		m_taaPreviousImages[1].reset();

		// Populate TAA jitter offsets with quasi-random sequence.
		for (size_t i = 0; i < m_taaJitterOffsets.size(); ++i)
		{
			m_taaJitterOffsets[i].x = (2.0f * Halton(i + 1, 2) - 1.0f) / size.x;
			m_taaJitterOffsets[i].y = (2.0f * Halton(i + 1, 3) - 1.0f) / size.y;
		}

		if (!CreateTAAImage(device, resourceFactory, size))
		{
			return false;
		}

		if (!m_taaMaterial->BindSampler(0, m_linearSampler) ||
			!m_taaMaterial->BindSampler(1, m_nearestSampler) ||
			!m_taaMaterial->BindImageView(2, inputImageView) ||
			!m_taaMaterial->BindImageView(3, m_taaPreviousImages[0]->GetView()) ||
			!m_taaMaterial->BindImageView(4, m_gBuffer.GetVelocityImageView()) ||
			!m_taaMaterial->BindImageView(5, m_gBuffer.GetDepthImageView()))
			return false;

		return true;
	}

	bool PostProcessing::CreateTAAImage(const IDevice& device, const IResourceFactory& resourceFactory, const glm::uvec2& size)
	{
		Format format = Format::R8G8B8A8Unorm;
		for (size_t i = 0; i < m_taaPreviousImages.size(); ++i)
		{
			ImageUsageFlags usageFlags = i == 0 ?
				ImageUsageFlags::Sampled | ImageUsageFlags::TransferDst :
				ImageUsageFlags::ColorAttachment | ImageUsageFlags::TransferSrc;

			m_taaPreviousImages[i] = std::move(resourceFactory.CreateRenderImage());
			glm::uvec3 extent(size.x, size.y, 1);
			if (!m_taaPreviousImages[i]->Initialise(device, ImageType::e2D, format, extent, 1, ImageTiling::Optimal, usageFlags,
				ImageAspectFlags::Color, MemoryUsage::AutoPreferDevice, AllocationCreateFlags::None, SharingMode::Exclusive))
			{
				Logger::Error("Failed to create image.");
				return false;
			}
		}

		return true;
	}

	void PostProcessing::TransitionTAAImageLayouts(const IDevice& device, const ICommandBuffer& commandBuffer) const
	{
		m_taaPreviousImages[0]->TransitionImageLayout(device, commandBuffer, ImageLayout::ShaderReadOnly);
		m_taaPreviousImages[1]->TransitionImageLayout(device, commandBuffer, ImageLayout::ColorAttachment);
	}

	void PostProcessing::BlitTAA(const IDevice& device, const ICommandBuffer& commandBuffer) const
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

	void PostProcessing::Draw(const ICommandBuffer& commandBuffer, uint32_t frameIndex) const
	{
		m_taaMaterial->BindMaterial(commandBuffer, frameIndex);

		uint32_t enabled = m_enabled ? 1 : 0;
		commandBuffer.PushConstants(m_taaMaterial, ShaderStageFlags::Vertex, 0, sizeof(uint32_t), &enabled);
		commandBuffer.Draw(3, 1, 0, 0);
	}
}