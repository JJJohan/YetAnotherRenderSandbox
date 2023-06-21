#include "GBuffer.hpp"
#include "Core/Logging/Logger.hpp"
#include "ShadowMap.hpp"
#include "OS/Files.hpp"
#include "Material.hpp"
#include "Resources/IMaterialManager.hpp"
#include "Resources/IDevice.hpp"
#include "Resources/IPhysicalDevice.hpp"
#include "Resources/IBuffer.hpp"
#include "Resources/ICommandBuffer.hpp"
#include "Resources/IImageSampler.hpp"
#include "Resources/IResourceFactory.hpp"
#include "Resources/AttachmentInfo.hpp"

using namespace Engine::Logging;
using namespace Engine::OS;

namespace Engine::Rendering
{
	GBuffer::GBuffer()
		: m_gBufferImages()
		, m_depthImage()
		, m_depthFormat(Format::Undefined)
		, m_outputImage()
		, m_imageFormats()
		, m_sampler()
		, m_shadowSampler()
		, m_combineMaterial(nullptr)
	{
	}

	const Format OutputImageFormat = Format::R8G8B8A8Unorm;

	bool GBuffer::CreateImageAndView(const IDevice& device, const IResourceFactory& resourceFactory, const glm::uvec2& size, Format format)
	{
		if (m_imageFormats.size() < GBUFFER_SIZE)
			m_imageFormats.emplace_back(format);

		std::unique_ptr<IRenderImage>& image = m_gBufferImages.emplace_back(std::move(resourceFactory.CreateRenderImage()));
		glm::uvec3 extent(size.x, size.y, 1);
		if (!image->Initialise(device, ImageType::e2D, format, extent, 1, ImageTiling::Optimal, ImageUsageFlags::ColorAttachment | ImageUsageFlags::Sampled,
			ImageAspectFlags::Color, MemoryUsage::AutoPreferDevice, AllocationCreateFlags::None, SharingMode::Exclusive))
		{
			Logger::Error("Failed to create image.");
			return false;
		}

		return true;
	}

	bool GBuffer::CreateColorImages(const IDevice& device, const IResourceFactory& resourceFactory, const glm::uvec2& size)
	{
		// Albedo
		if (!CreateImageAndView(device, resourceFactory, size, Format::R8G8B8A8Unorm))
		{
			return false;
		}

		// Normals
		if (!CreateImageAndView(device, resourceFactory, size, Format::R16G16B16A16Sfloat))
		{
			return false;
		}

		// WorldPos
		if (!CreateImageAndView(device, resourceFactory, size, Format::R16G16B16A16Sfloat))
		{
			return false;
		}

		// Metal & Roughness
		if (!CreateImageAndView(device, resourceFactory, size, Format::R8G8Unorm))
		{
			return false;
		}

		// Velocity
		if (!CreateImageAndView(device, resourceFactory, size, Format::R16G16Sfloat))
		{
			return false;
		}

		return true;
	}

	bool GBuffer::CreateOutputImage(const IDevice& device, const IResourceFactory& resourceFactory, const glm::uvec2& size)
	{
		m_outputImage = std::move(resourceFactory.CreateRenderImage());
		glm::uvec3 extent(size.x, size.y, 1);
		if (!m_outputImage->Initialise(device, ImageType::e2D, OutputImageFormat, extent, 1, ImageTiling::Optimal, ImageUsageFlags::ColorAttachment | ImageUsageFlags::Sampled,
			ImageAspectFlags::Color, MemoryUsage::AutoPreferDevice, AllocationCreateFlags::None, SharingMode::Exclusive))
		{
			Logger::Error("Failed to create image.");
			return false;
		}

		return true;
	}

	bool GBuffer::CreateDepthImage(const IDevice& device, const IResourceFactory& resourceFactory, const glm::uvec2& size)
	{
		if (m_depthFormat == Format::Undefined)
		{
			Logger::Error("Failed to find suitable format for depth texture.");
			return false;
		}

		m_depthImage = std::move(resourceFactory.CreateRenderImage());
		glm::uvec3 extent(size.x, size.y, 1);
		if (!m_depthImage->Initialise(device, ImageType::e2D, m_depthFormat, extent, 1, ImageTiling::Optimal, ImageUsageFlags::DepthStencilAttachment | ImageUsageFlags::Sampled,
			ImageAspectFlags::Depth, MemoryUsage::AutoPreferDevice, AllocationCreateFlags::None, SharingMode::Exclusive))
		{
			Logger::Error("Failed to create depth image.");
			return false;
		}

		return true;
	}

	bool GBuffer::Initialise(const IPhysicalDevice& physicalDevice, const IDevice& device,
		const IMaterialManager& materialManager, const IResourceFactory& resourceFactory,
		Format depthFormat, const glm::uvec2& size, const std::vector<std::unique_ptr<IBuffer>>& frameInfoBuffers,
		const std::vector<std::unique_ptr<IBuffer>>& lightBuffers, const ShadowMap& shadowMap)
	{
		uint32_t cascades = shadowMap.GetCascadeCount();

		m_depthFormat = depthFormat;

		if (!materialManager.TryGetMaterial("Combine", &m_combineMaterial))
		{
			return false;
		}

		m_sampler = std::move(resourceFactory.CreateImageSampler());
		if (!m_sampler->Initialise(device, Filter::Linear, Filter::Linear, SamplerMipmapMode::Linear,
			SamplerAddressMode::Repeat, 1))
		{
			return false;
		}

		m_shadowSampler = std::move(resourceFactory.CreateImageSampler());
		if (!m_shadowSampler->Initialise(device, Filter::Linear, Filter::Linear, SamplerMipmapMode::Linear,
			SamplerAddressMode::ClampToBorder, 1))
		{
			return false;
		}

		if (!Rebuild(physicalDevice, device, resourceFactory, size, frameInfoBuffers, lightBuffers, shadowMap))
		{
			return false;
		}

		return true;
	}

	uint64_t GBuffer::GetMemoryUsage() const
	{
		uint64_t totalSize = 0;
		const glm::uvec3& extents = m_outputImage->GetDimensions();

		for (const auto& image : m_gBufferImages)
		{
			totalSize += GetSizeForFormat(image->GetFormat()) * extents.x * extents.y * extents.z;
		}

		totalSize += GetSizeForFormat(m_depthImage->GetFormat()) * extents.x * extents.y * extents.z;
		totalSize += GetSizeForFormat(m_outputImage->GetFormat()) * extents.x * extents.y * extents.z;

		return totalSize;
	}

	bool GBuffer::Rebuild(const IPhysicalDevice& physicalDevice, const IDevice& device, const IResourceFactory& resourceFactory,
		const glm::uvec2& size, const std::vector<std::unique_ptr<IBuffer>>& frameInfoBuffers,
		const std::vector<std::unique_ptr<IBuffer>>& lightBuffers, const ShadowMap& shadowMap)
	{
		m_depthImage.reset();
		m_outputImage.reset();
		m_gBufferImages.clear();
		m_imageFormats.clear();

		if (!CreateDepthImage(device, resourceFactory, size))
		{
			return false;
		}

		if (!CreateColorImages(device, resourceFactory, size))
		{
			return false;
		}

		if (!CreateOutputImage(device, resourceFactory, size))
		{
			return false;
		}

		std::vector<const IImageView*> imageViews(m_gBufferImages.size());
		for (size_t i = 0; i < m_gBufferImages.size(); ++i)
			imageViews[i] = &m_gBufferImages[i]->GetView();

		if (!m_combineMaterial->BindUniformBuffers(0, frameInfoBuffers) ||
			!m_combineMaterial->BindUniformBuffers(1, lightBuffers) ||
			!m_combineMaterial->BindSampler(2, m_sampler) ||
			!m_combineMaterial->BindImageViews(3, imageViews) ||
			!m_combineMaterial->BindSampler(4, m_shadowSampler) ||
			!m_combineMaterial->BindImageViews(5, shadowMap.GetShadowImageViews()))
			return false;

		return true;
	}

	void GBuffer::TransitionImageLayouts(const IDevice& device, const ICommandBuffer& commandBuffer, ImageLayout newLayout)
	{
		for (size_t i = 0; i < GBUFFER_SIZE; ++i)
		{
			m_gBufferImages[i]->TransitionImageLayout(device, commandBuffer, newLayout);
		}
	}

	void GBuffer::TransitionDepthLayout(const IDevice& device, const ICommandBuffer& commandBuffer, ImageLayout newLayout)
	{
		m_depthImage->TransitionImageLayout(device, commandBuffer, newLayout);
	}

	std::vector<AttachmentInfo> GBuffer::GetRenderAttachments() const
	{
		std::vector<AttachmentInfo> attachments(GBUFFER_SIZE);

		for (uint32_t i = 0; i < GBUFFER_SIZE; ++i)
		{
			attachments[i] = AttachmentInfo(&m_gBufferImages[i]->GetView(), ImageLayout::ColorAttachment, AttachmentLoadOp::Clear, AttachmentStoreOp::Store);
		}

		return attachments;
	}

	AttachmentInfo GBuffer::GetDepthAttachment() const
	{
		return AttachmentInfo(&m_depthImage->GetView(), ImageLayout::DepthAttachment, AttachmentLoadOp::Clear, AttachmentStoreOp::Store, ClearValue(1.0f));
	}

	void GBuffer::SetDebugMode(uint32_t value) const
	{
		m_combineMaterial->SetSpecialisationConstant("debugMode", static_cast<int32_t>(value));
	}

	void GBuffer::DrawFinalImage(const ICommandBuffer& commandBuffer, uint32_t frameIndex) const
	{
		m_combineMaterial->BindMaterial(commandBuffer, frameIndex);
		commandBuffer.Draw(3, 1, 0, 0);
	}
}