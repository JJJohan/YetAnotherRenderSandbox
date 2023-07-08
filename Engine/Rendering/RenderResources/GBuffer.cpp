#include "GBuffer.hpp"
#include "Core/Logging/Logger.hpp"
#include "../IDevice.hpp"
#include "../IPhysicalDevice.hpp"
#include "../IResourceFactory.hpp"
#include "../Resources/AttachmentInfo.hpp"
#include "../Renderer.hpp"

using namespace Engine::Logging;

namespace Engine::Rendering
{
	GBuffer::GBuffer()
		: IRenderResource("GBuffer")
		, m_gBufferImages()
		, m_depthImage()
		, m_imageFormats()
	{
		m_imageOutputs = 
		{
			"Albedo",
			"WorldNormal",
			"WorldPos",
			"MetalRoughness",
			"Velocity",
			"Depth"
		};
	}

	bool GBuffer::CreateImageAndView(const IDevice& device, const IResourceFactory& resourceFactory, const glm::uvec2& size, Format format)
	{
		if (m_imageFormats.size() < GBUFFER_SIZE)
			m_imageFormats.emplace_back(format);

		std::unique_ptr<IRenderImage>& image = m_gBufferImages.emplace_back(std::move(resourceFactory.CreateRenderImage()));
		glm::uvec3 extent(size.x, size.y, 1);
		if (!image->Initialise(device, ImageType::e2D, format, extent, 1, 1, ImageTiling::Optimal,
			ImageUsageFlags::ColorAttachment | ImageUsageFlags::Sampled, ImageAspectFlags::Color, MemoryUsage::AutoPreferDevice,
			AllocationCreateFlags::None, SharingMode::Exclusive))
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

	bool GBuffer::CreateDepthImage(const IDevice& device, const IResourceFactory& resourceFactory, const glm::uvec2& size, Format format)
	{
		if (format == Format::Undefined)
		{
			Logger::Error("Failed to find suitable format for depth texture.");
			return false;
		}

		m_depthImage = std::move(resourceFactory.CreateRenderImage());
		glm::uvec3 extent(size.x, size.y, 1);
		if (!m_depthImage->Initialise(device, ImageType::e2D, format, extent, 1, 1,
			ImageTiling::Optimal, ImageUsageFlags::DepthStencilAttachment | ImageUsageFlags::Sampled,
			ImageAspectFlags::Depth, MemoryUsage::AutoPreferDevice, AllocationCreateFlags::None, SharingMode::Exclusive))
		{
			Logger::Error("Failed to create depth image.");
			return false;
		}

		return true;
	}

	uint64_t GBuffer::GetMemoryUsage() const
	{
		uint64_t totalSize = 0;
		const glm::uvec3& extents = m_depthImage->GetDimensions();

		for (const auto& image : m_gBufferImages)
		{
			totalSize += GetSizeForFormat(image->GetFormat()) * extents.x * extents.y * extents.z;
		}

		totalSize += GetSizeForFormat(m_depthImage->GetFormat()) * extents.x * extents.y * extents.z;

		return totalSize;
	}

	bool GBuffer::Build(const Renderer& renderer)
	{
		m_depthImage.reset();
		m_gBufferImages.clear();
		m_imageFormats.clear();
		m_imageResources.clear();

		const IDevice& device = renderer.GetDevice();
		const IResourceFactory& resourceFactory = renderer.GetResourceFactory();
		const ISwapChain& swapChain = renderer.GetSwapChain();
		const glm::uvec2& size = swapChain.GetExtent();
		Format depthFormat = renderer.GetDepthFormat();

		if (!CreateDepthImage(device, resourceFactory, size, depthFormat))
		{
			return false;
		}

		if (!CreateColorImages(device, resourceFactory, size))
		{
			return false;
		}

		m_imageResources["Albedo"] = m_gBufferImages[0].get();
		m_imageResources["WorldNormal"] = m_gBufferImages[1].get();
		m_imageResources["WorldPos"] = m_gBufferImages[2].get();
		m_imageResources["MetalRoughness"] = m_gBufferImages[3].get();
		m_imageResources["Velocity"] = m_gBufferImages[4].get();
		m_imageResources["Depth"] = m_depthImage.get();

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
}