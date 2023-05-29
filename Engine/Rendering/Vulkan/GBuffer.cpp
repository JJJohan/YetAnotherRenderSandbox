#include "GBuffer.hpp"
#include "Core/Logging/Logger.hpp"
#include "ImageView.hpp"
#include "RenderImage.hpp"
#include "DescriptorPool.hpp"
#include "Device.hpp"
#include "Buffer.hpp"
#include "ShadowMap.hpp"
#include "ImageSampler.hpp"
#include "FrameInfoUniformBuffer.hpp"
#include "LightUniformBuffer.hpp"
#include "OS/Files.hpp"
#include "PipelineLayout.hpp"
#include <vma/vk_mem_alloc.h>

using namespace Engine::Logging;
using namespace Engine::OS;

namespace Engine::Rendering::Vulkan
{
	GBuffer::GBuffer(uint32_t concurrentFrames)
		: m_concurrentFrames(concurrentFrames)
		, m_gBufferImages()
		, m_gBufferImageViews()
		, m_depthImage()
		, m_depthImageView()
		, m_depthFormat(vk::Format::eUndefined)
		, m_imageFormats()
		, m_combineShader()
		, m_descriptorSets()
		, m_descriptorPool()
		, m_sampler()
		, m_shadowSampler()
	{
	}

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
		if (!CreateImageAndView(device, allocator, size, vk::Format::eR32G32B32A32Sfloat))
		{
			return false;
		}

		// WorldPos
		if (!CreateImageAndView(device, allocator, size, vk::Format::eR32G32B32A32Sfloat))
		{
			return false;
		}

		// Metal & Roughness
		if (!CreateImageAndView(device, allocator, size, vk::Format::eR8G8Unorm))
		{
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
		if (!m_depthImage->Initialise(vk::ImageType::e2D, m_depthFormat, extent, 1, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment,
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

	bool GBuffer::SetupDescriptorSetLayout(const Device& device, uint32_t shadowCascades)
	{
		std::array<vk::DescriptorSetLayoutBinding, 6> layoutBindings =
		{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment),
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment),
			vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eSampler, 1, vk::ShaderStageFlagBits::eFragment),
			vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eSampledImage, GBUFFER_SIZE, vk::ShaderStageFlagBits::eFragment),
			vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eSampler, 1, vk::ShaderStageFlagBits::eFragment),
			vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eSampledImage, shadowCascades, vk::ShaderStageFlagBits::eFragment)
		};

		vk::DescriptorSetLayoutCreateInfo layoutInfo(vk::DescriptorSetLayoutCreateFlags(), layoutBindings);

		const vk::Device& deviceImp = device.Get();
		m_descriptorSetLayout = deviceImp.createDescriptorSetLayoutUnique(layoutInfo);
		if (!m_descriptorSetLayout.get())
		{
			Logger::Error("Failed to create descriptor set layout.");
			return false;
		}

		return true;
	}

	bool GBuffer::Initialise(const Device& device, VmaAllocator allocator, vk::Format swapChainFormat, vk::Format depthFormat, float maxAnisotoropic,
		const std::vector<std::unique_ptr<Buffer>>& frameInfoBuffers, const std::vector<std::unique_ptr<Buffer>>& lightBuffers,
		const ShadowMap& shadowMap, const glm::uvec2& size)
	{
		uint32_t cascades = shadowMap.GetCascadeCount();
		if (!SetupDescriptorSetLayout(device, cascades))
		{
			return false;
		}

		std::vector<vk::DescriptorPoolSize> poolSizes =
		{
			vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1),
			vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1),
			vk::DescriptorPoolSize(vk::DescriptorType::eSampler, 1),
			vk::DescriptorPoolSize(vk::DescriptorType::eSampledImage, GBUFFER_SIZE),
			vk::DescriptorPoolSize(vk::DescriptorType::eSampler, 1),
			vk::DescriptorPoolSize(vk::DescriptorType::eSampledImage, 4)
		};

		m_descriptorPool = std::make_unique<DescriptorPool>();
		if (!m_descriptorPool->Initialise(device, m_concurrentFrames, poolSizes))
		{
			return false;
		}

		std::vector<vk::DescriptorSetLayout> layouts;
		for (uint32_t i = 0; i < m_concurrentFrames; ++i)
		{
			layouts.push_back(m_descriptorSetLayout.get());
		}

		m_depthFormat = depthFormat;
		m_descriptorSets = m_descriptorPool->CreateDescriptorSets(device, layouts);

		std::string vertFilePath = "Shaders/Combine_vert.spv";
		std::vector<uint8_t> vertData;
		if (!Files::TryReadFile(vertFilePath, vertData))
		{
			Logger::Error("Failed to read shader program at path '{}'.", vertFilePath);
			return false;
		}

		std::string fragFilePath = "Shaders/Combine_frag.spv";
		std::vector<uint8_t> fragData;
		if (!Files::TryReadFile(fragFilePath, fragData))
		{
			Logger::Error("Failed to read shader program at path '{}'.", fragFilePath);
			return false;
		}

		auto programs = std::unordered_map<vk::ShaderStageFlagBits, std::vector<uint8_t>>{
			{ vk::ShaderStageFlagBits::eVertex, vertData },
			{ vk::ShaderStageFlagBits::eFragment, fragData }
		};

		std::vector<vk::VertexInputBindingDescription> bindingDescriptions = {};
		std::vector<vk::VertexInputAttributeDescription> attributeDescriptions = {};
		std::vector<vk::DescriptorSetLayout> descriptorSetLayouts = { m_descriptorSetLayout.get() };

		std::vector<vk::Format> attachmentFormats = { swapChainFormat };

		m_combineShader = std::make_unique<PipelineLayout>();
		if (!m_combineShader->Initialise(device, "Combine", programs, bindingDescriptions, attributeDescriptions, attachmentFormats, vk::Format::eUndefined, descriptorSetLayouts))
		{
			return false;
		}

		m_sampler = std::make_unique<ImageSampler>();
		if (!m_sampler->Initialise(device, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear,
			vk::SamplerAddressMode::eRepeat, maxAnisotoropic))
		{
			return false;
		}

		m_shadowSampler = std::make_unique<ImageSampler>();
		if (!m_shadowSampler->Initialise(device, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear,
			vk::SamplerAddressMode::eClampToBorder, maxAnisotoropic))
		{
			return false;
		}

		if (!Rebuild(device, allocator, size, swapChainFormat, frameInfoBuffers, lightBuffers, shadowMap, false))
		{
			return false;
		}

		return true;
	}

	bool GBuffer::Rebuild(const Device& device, VmaAllocator allocator,
		const glm::uvec2& size, vk::Format swapChainFormat, const std::vector<std::unique_ptr<Buffer>>& frameInfoBuffers,
		const std::vector<std::unique_ptr<Buffer>>& lightBuffers, const ShadowMap& shadowMap, bool rebuildPipeline)
	{
		m_depthImageView.reset();
		m_depthImage.reset();
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

		std::array<vk::DescriptorImageInfo, 1> samplerInfos =
		{
			vk::DescriptorImageInfo(m_sampler->Get(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal)
		};

		std::array<vk::DescriptorImageInfo, 1> shadowSamplerInfos =
		{
			vk::DescriptorImageInfo(m_shadowSampler->Get(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal)
		};

		std::vector<vk::DescriptorImageInfo> imageInfos;
		imageInfos.resize(GBUFFER_SIZE);

		for (uint32_t i = 0; i < GBUFFER_SIZE; ++i)
		{
			imageInfos[i] = vk::DescriptorImageInfo(nullptr, m_gBufferImageViews[i]->Get(), vk::ImageLayout::eShaderReadOnlyOptimal);
		}

		uint32_t cascades = shadowMap.GetCascadeCount();
		std::vector<vk::DescriptorImageInfo> shadowImageInfo(cascades);
		for (uint32_t i = 0; i < cascades; ++i)
		{
			shadowImageInfo[i] = vk::DescriptorImageInfo(nullptr, shadowMap.GetShadowImageView(i).Get(), vk::ImageLayout::eShaderReadOnlyOptimal);
		}

		for (uint32_t i = 0; i < m_concurrentFrames; ++i)
		{
			std::array<vk::DescriptorBufferInfo, 1> frameBufferInfos =
			{
				vk::DescriptorBufferInfo(frameInfoBuffers[i]->Get(), 0, sizeof(FrameInfoUniformBuffer))
			};

			std::array<vk::DescriptorBufferInfo, 1> lightBufferInfos =
			{
				vk::DescriptorBufferInfo(lightBuffers[i]->Get(), 0, sizeof(LightUniformBuffer))
			};

			std::array<vk::WriteDescriptorSet, 6> writeDescriptorSets =
			{
				vk::WriteDescriptorSet(m_descriptorSets[i], 0, 0, vk::DescriptorType::eUniformBuffer, nullptr, frameBufferInfos),
				vk::WriteDescriptorSet(m_descriptorSets[i], 1, 0, vk::DescriptorType::eUniformBuffer, nullptr, lightBufferInfos),
				vk::WriteDescriptorSet(m_descriptorSets[i], 2, 0, vk::DescriptorType::eSampler, samplerInfos),
				vk::WriteDescriptorSet(m_descriptorSets[i], 3, 0, vk::DescriptorType::eSampledImage, imageInfos),
				vk::WriteDescriptorSet(m_descriptorSets[i], 4, 0, vk::DescriptorType::eSampler, shadowSamplerInfos),
				vk::WriteDescriptorSet(m_descriptorSets[i], 5, 0, vk::DescriptorType::eSampledImage, shadowImageInfo)
			};

			device.Get().updateDescriptorSets(writeDescriptorSets, nullptr);
		}

		if (rebuildPipeline)
		{
			std::vector<vk::Format> attachmentFormats = { swapChainFormat };

			std::vector<vk::DescriptorSetLayout> descriptorSetLayouts = { m_descriptorSetLayout.get() };

			PipelineLayout* pipelineLayout = static_cast<PipelineLayout*>(m_combineShader.get());
			if (!pipelineLayout->Rebuild(device, attachmentFormats, vk::Format::eUndefined, descriptorSetLayouts))
			{
				return false;
			}
		}

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

	std::vector<vk::ImageView> GBuffer::GetImageViews() const
	{
		std::vector<vk::ImageView> imageViews(GBUFFER_SIZE);
		for (size_t i = 0; i < GBUFFER_SIZE; ++i)
		{
			imageViews[i] = m_gBufferImageViews[i]->Get();
		}

		return imageViews;
	}

	void GBuffer::DrawFinalImage(const vk::CommandBuffer& commandBuffer, uint32_t frameIndex) const
	{
		const vk::Pipeline& graphicsPipeline = m_combineShader->GetGraphicsPipeline();
		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);

		commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_combineShader->Get(), 0, m_descriptorSets[frameIndex], nullptr);
		commandBuffer.draw(3, 1, 0, 0);
	}

	const std::vector<vk::Format>& GBuffer::GetImageFormats() const
	{
		return m_imageFormats;
	}

	vk::Format GBuffer::GetDepthFormat() const
	{
		return m_depthFormat;
	}
}