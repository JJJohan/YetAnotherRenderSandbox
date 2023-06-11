#include "GBuffer.hpp"
#include "Core/Logging/Logger.hpp"
#include "Device.hpp"
#include "PhysicalDevice.hpp"
#include "Buffer.hpp"
#include "ShadowMap.hpp"
#include "ImageSampler.hpp"
#include "FrameInfoUniformBuffer.hpp"
#include "LightUniformBuffer.hpp"
#include "OS/Files.hpp"
#include "PipelineLayout.hpp"
#include "VulkanFormatInterop.hpp"
#include "PipelineManager.hpp"

using namespace Engine::Logging;
using namespace Engine::OS;

namespace Engine::Rendering::Vulkan
{
	GBuffer::GBuffer()
		: m_gBufferImages()
		, m_gBufferImageViews()
		, m_depthImage()
		, m_depthImageView()
		, m_depthFormat(vk::Format::eUndefined)
		, m_outputImage()
		, m_outputImageView()
		, m_imageFormats()
		, m_sampler()
		, m_shadowSampler()
		, m_combineShader(nullptr)
	{
	}

	const vk::Format OutputImageFormat = vk::Format::eR8G8B8A8Unorm;

	bool GBuffer::CreateImageAndView(const Device& device, VmaAllocator allocator, const glm::uvec2& size, vk::Format format)
	{
		if (m_imageFormats.size() < GBUFFER_SIZE)
			m_imageFormats.emplace_back(format);

		std::unique_ptr<RenderImage>& image = m_gBufferImages.emplace_back(std::make_unique<RenderImage>(allocator));
		vk::Extent3D extent(size.x, size.y, 1);
		if (!image->Initialise(vk::ImageType::e2D, format, extent, 1, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
			VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0, vk::SharingMode::eExclusive))
		{
			Logger::Error("Failed to create image.");
			return false;
		}

		std::unique_ptr<ImageView>& imageView = m_gBufferImageViews.emplace_back(std::make_unique<ImageView>());
		if (!imageView->Initialise(device, image->Get(), 1, format, vk::ImageAspectFlagBits::eColor))
		{
			Logger::Error("Failed to create image view.");
			return false;
		}

		return true;
	}

	bool GBuffer::CreateColorImages(const Device& device, VmaAllocator allocator, const glm::uvec2& size)
	{
		// Albedo
		if (!CreateImageAndView(device, allocator, size, vk::Format::eR8G8B8A8Unorm))
		{
			return false;
		}

		// Normals
		if (!CreateImageAndView(device, allocator, size, vk::Format::eR16G16B16A16Sfloat))
		{
			return false;
		}

		// WorldPos
		if (!CreateImageAndView(device, allocator, size, vk::Format::eR16G16B16A16Sfloat))
		{
			return false;
		}

		// Metal & Roughness
		if (!CreateImageAndView(device, allocator, size, vk::Format::eR8G8Unorm))
		{
			return false;
		}

		// Velocity
		if (!CreateImageAndView(device, allocator, size, vk::Format::eR16G16Sfloat))
		{
			return false;
		}

		return true;
	}

	bool GBuffer::CreateOutputImage(const Device& device, VmaAllocator allocator, const glm::uvec2& size)
	{
		m_outputImage = std::make_unique<RenderImage>(allocator);
		vk::Extent3D extent(size.x, size.y, 1);
		if (!m_outputImage->Initialise(vk::ImageType::e2D, OutputImageFormat, extent, 1, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
			VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0, vk::SharingMode::eExclusive))
		{
			Logger::Error("Failed to create image.");
			return false;
		}

		m_outputImageView = std::make_unique<ImageView>();
		if (!m_outputImageView->Initialise(device, m_outputImage->Get(), 1, OutputImageFormat, vk::ImageAspectFlagBits::eColor))
		{
			Logger::Error("Failed to create image view.");
			return false;
		}

		return true;
	}

	bool GBuffer::CreateDepthImage(const Device& device, VmaAllocator allocator, const glm::uvec2& size)
	{
		if (m_depthFormat == vk::Format::eUndefined)
		{
			Logger::Error("Failed to find suitable format for depth texture.");
			return false;
		}

		m_depthImage = std::make_unique<RenderImage>(allocator);
		vk::Extent3D extent(size.x, size.y, 1);
		if (!m_depthImage->Initialise(vk::ImageType::e2D, m_depthFormat, extent, 1, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
			VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0, vk::SharingMode::eExclusive))
		{
			Logger::Error("Failed to create depth image.");
			return false;
		}

		m_depthImageView = std::make_unique<ImageView>();
		if (!m_depthImageView->Initialise(device, m_depthImage->Get(), 1, m_depthFormat, vk::ImageAspectFlagBits::eDepth))
		{
			Logger::Error("Failed to create depth image view.");
			return false;
		}

		return true;
	}

	bool GBuffer::Initialise(const PhysicalDevice& physicalDevice, const Device& device,
		const PipelineManager& pipelineManager, VmaAllocator allocator,
		vk::Format depthFormat, const glm::uvec2& size, const std::vector<std::unique_ptr<Buffer>>& frameInfoBuffers,
		const std::vector<std::unique_ptr<Buffer>>& lightBuffers, const ShadowMap& shadowMap)
	{
		uint32_t cascades = shadowMap.GetCascadeCount();

		m_depthFormat = depthFormat;

		if (!pipelineManager.TryGetPipelineLayout("Combine", &m_combineShader))
		{
			return false;
		}

		m_sampler = std::make_unique<ImageSampler>();
		if (!m_sampler->Initialise(device, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear,
			vk::SamplerAddressMode::eRepeat, 1))
		{
			return false;
		}

		m_shadowSampler = std::make_unique<ImageSampler>();
		if (!m_shadowSampler->Initialise(device, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear,
			vk::SamplerAddressMode::eClampToBorder, 1))
		{
			return false;
		}

		if (!Rebuild(physicalDevice, device, allocator, size, frameInfoBuffers, lightBuffers, shadowMap))
		{
			return false;
		}

		return true;
	}

	uint64_t GBuffer::GetMemoryUsage() const
	{
		uint64_t totalSize = 0;
		const vk::Extent3D& extents = m_outputImage->GetDimensions();

		for (const auto& image : m_gBufferImages)
		{
			totalSize += GetSizeForFormat(image->GetFormat()) * extents.width * extents.height * extents.depth;
		}

		totalSize += GetSizeForFormat(m_depthImage->GetFormat()) * extents.width * extents.height * extents.depth;
		totalSize += GetSizeForFormat(m_outputImage->GetFormat()) * extents.width * extents.height * extents.depth;

		return totalSize;
	}

	bool GBuffer::Rebuild(const PhysicalDevice& physicalDevice, const Device& device, VmaAllocator allocator,
		const glm::uvec2& size, const std::vector<std::unique_ptr<Buffer>>& frameInfoBuffers,
		const std::vector<std::unique_ptr<Buffer>>& lightBuffers, const ShadowMap& shadowMap)
	{
		m_depthImageView.reset();
		m_depthImage.reset();
		m_outputImageView.reset();
		m_outputImage.reset();
		m_gBufferImageViews.clear();
		m_gBufferImages.clear();
		m_imageFormats.clear();

		if (!CreateDepthImage(device, allocator, size))
		{
			return false;
		}

		if (!CreateColorImages(device, allocator, size))
		{
			return false;
		}

		if (!CreateOutputImage(device, allocator, size))
		{
			return false;
		}

		if (!m_combineShader->BindUniformBuffers(0, frameInfoBuffers) ||
			!m_combineShader->BindUniformBuffers(1, lightBuffers) ||
			!m_combineShader->BindSampler(2, m_sampler) ||
			!m_combineShader->BindImageViews(3, m_gBufferImageViews) ||
			!m_combineShader->BindSampler(4, m_shadowSampler) ||
			!m_combineShader->BindImageViews(5, shadowMap.GetShadowImageViews()))
			return false;

		return true;
	}

	void GBuffer::TransitionImageLayouts(const Device& device, const vk::CommandBuffer& commandBuffer, vk::ImageLayout newLayout)
	{
		for (size_t i = 0; i < GBUFFER_SIZE; ++i)
		{
			m_gBufferImages[i]->TransitionImageLayout(device, commandBuffer, newLayout);
		}
	}

	void GBuffer::TransitionDepthLayout(const Device& device, const vk::CommandBuffer& commandBuffer, vk::ImageLayout newLayout)
	{
		m_depthImage->TransitionImageLayout(device, commandBuffer, newLayout);
	}

	std::vector<vk::RenderingAttachmentInfo> GBuffer::GetRenderAttachments() const
	{
		std::vector<vk::RenderingAttachmentInfo> attachments(GBUFFER_SIZE);

		for (uint32_t i = 0; i < GBUFFER_SIZE; ++i)
		{
			attachments[i] = vk::RenderingAttachmentInfo{};
			attachments[i].imageView = m_gBufferImageViews[i]->Get();
			attachments[i].imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
			attachments[i].loadOp = vk::AttachmentLoadOp::eClear;
			attachments[i].storeOp = vk::AttachmentStoreOp::eStore;
		}

		return attachments;
	}

	vk::RenderingAttachmentInfo GBuffer::GetDepthAttachment() const
	{
		vk::RenderingAttachmentInfo depthAttachmentInfo{};
		depthAttachmentInfo.imageView = m_depthImageView->Get();
		depthAttachmentInfo.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
		depthAttachmentInfo.loadOp = vk::AttachmentLoadOp::eClear;
		depthAttachmentInfo.storeOp = vk::AttachmentStoreOp::eStore;
		depthAttachmentInfo.clearValue = vk::ClearValue(vk::ClearDepthStencilValue(1.0f, 0));
		return depthAttachmentInfo;
	}

	void GBuffer::SetDebugMode(uint32_t value) const
	{
		m_combineShader->SetSpecialisationConstant("debugMode", static_cast<int32_t>(value));
	}

	void GBuffer::DrawFinalImage(const vk::CommandBuffer& commandBuffer, uint32_t frameIndex) const
	{
		m_combineShader->BindPipeline(commandBuffer, frameIndex);
		commandBuffer.draw(3, 1, 0, 0);
	}
}