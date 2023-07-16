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
			{"Albedo", nullptr},
			{"WorldNormal", nullptr},
			{"WorldPos", nullptr},
			{"MetalRoughness", nullptr},
			{"Velocity", nullptr},
			{"Depth", nullptr}
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

	size_t GBuffer::GetMemoryUsage() const
	{
		size_t totalSize = 0;
		const glm::uvec3& extents = m_depthImage->GetDimensions();

		for (const auto& image : m_gBufferImages)
		{
			totalSize += GetSizeForFormat(image->GetFormat()) * extents.x * extents.y * extents.z;
		}

		totalSize += GetSizeForFormat(m_depthImage->GetFormat()) * extents.x * extents.y * extents.z;

		return totalSize;
	}

	bool GBuffer::Build(const Renderer& renderer, const std::unordered_map<const char*, IRenderImage*>& imageInputs,
		const std::unordered_map<const char*, IBuffer*>& bufferInputs)
	{
		m_depthImage.reset();
		m_gBufferImages.clear();
		m_imageFormats.clear();

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

		m_imageOutputs["Albedo"] = m_gBufferImages[0].get();
		m_imageOutputs["WorldNormal"] = m_gBufferImages[1].get();
		m_imageOutputs["WorldPos"] = m_gBufferImages[2].get();
		m_imageOutputs["MetalRoughness"] = m_gBufferImages[3].get();
		m_imageOutputs["Velocity"] = m_gBufferImages[4].get();
		m_imageOutputs["Depth"] = m_depthImage.get();

		return true;
	}
}