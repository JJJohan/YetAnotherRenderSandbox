#include "PostProcessing.hpp"
#include "Core/Logging/Logger.hpp"
#include "RenderImage.hpp"
#include "ImageView.hpp"
#include "ImageSampler.hpp"
#include "DescriptorPool.hpp"
#include "Device.hpp"
#include "GBuffer.hpp"
#include "PhysicalDevice.hpp"
#include "PipelineLayout.hpp"
#include "OS/Files.hpp"

using namespace Engine::Logging;
using namespace Engine::OS;

namespace Engine::Rendering::Vulkan
{
	PostProcessing::PostProcessing(GBuffer& gBuffer, uint32_t concurrentFrames)
		: m_concurrentFrames(concurrentFrames)
		, m_linearSampler()
		, m_nearestSampler()
		, m_enabled(true)
		, m_taaJitterOffsets()
		, m_taaShader()
		, m_taaPreviousImages()
		, m_taaPreviousImageViews()
		, m_taaDescriptorSets()
		, m_taaDescriptorSetLayout()
		, m_taaDescriptorPool()
		, m_taaFrameIndex(0)
		, m_gBuffer(gBuffer)
	{
	}

	bool PostProcessing::Initialise(const PhysicalDevice& physicalDevice, const Device& device,
		VmaAllocator allocator, vk::Format swapChainFormat, const glm::uvec2& size)
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

		if (!InitialiseTAA(physicalDevice, device, allocator, swapChainFormat, size))
		{
			return false;
		}

		if (!Rebuild(physicalDevice, device, allocator, swapChainFormat, size, false))
		{
			return false;
		}

		return true;
	}

	bool PostProcessing::InitialiseTAA(const PhysicalDevice& physicalDevice, const Device& device,
		VmaAllocator allocator, vk::Format swapChainFormat, const glm::uvec2& size)
	{
		if (!SetupTAADescriptorSetLayout(device))
		{
			return false;
		}

		std::vector<vk::DescriptorPoolSize> poolSizes =
		{
			vk::DescriptorPoolSize(vk::DescriptorType::eSampler, 1),
			vk::DescriptorPoolSize(vk::DescriptorType::eSampler, 1),
			vk::DescriptorPoolSize(vk::DescriptorType::eSampledImage, 1),
			vk::DescriptorPoolSize(vk::DescriptorType::eSampledImage, 1),
			vk::DescriptorPoolSize(vk::DescriptorType::eSampledImage, 1),
			vk::DescriptorPoolSize(vk::DescriptorType::eSampledImage, 1)
		};

		m_taaDescriptorPool = std::make_unique<DescriptorPool>();
		if (!m_taaDescriptorPool->Initialise(device, m_concurrentFrames, poolSizes))
		{
			return false;
		}

		std::vector<vk::DescriptorSetLayout> layouts;
		for (uint32_t i = 0; i < m_concurrentFrames; ++i)
		{
			layouts.push_back(m_taaDescriptorSetLayout.get());
		}

		m_taaDescriptorSets = m_taaDescriptorPool->CreateDescriptorSets(device, layouts);

		std::string vertFilePath = "Shaders/TAA_vert.spv";
		std::vector<uint8_t> vertData;
		if (!Files::TryReadFile(vertFilePath, vertData))
		{
			Logger::Error("Failed to read shader program at path '{}'.", vertFilePath);
			return false;
		}

		std::string fragFilePath = "Shaders/TAA_frag.spv";
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
		std::vector<vk::DescriptorSetLayout> descriptorSetLayouts = { m_taaDescriptorSetLayout.get() };
		std::vector<vk::PushConstantRange> pushConstantRanges = { vk::PushConstantRange(vk::ShaderStageFlagBits::eVertex, 0, sizeof(uint32_t)) };

		std::vector<vk::Format> attachmentFormats = { swapChainFormat, vk::Format::eR8G8B8A8Unorm };

		m_taaShader = std::make_unique<PipelineLayout>();
		if (!m_taaShader->Initialise(physicalDevice, device, "TAA", programs, bindingDescriptions,
			attributeDescriptions, attachmentFormats, vk::Format::eUndefined, descriptorSetLayouts, pushConstantRanges))
		{
			return false;
		}

		return true;
	}

	bool PostProcessing::Rebuild(const PhysicalDevice& physicalDevice, const Device& device,
		VmaAllocator allocator, vk::Format swapChainFormat, const glm::uvec2& size, bool rebuildPipeline)
	{
		if (!RebuildTAA(physicalDevice, device, allocator, swapChainFormat, m_gBuffer.GetOutputImageView(), size, rebuildPipeline))
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
		VmaAllocator allocator, vk::Format swapChainFormat, const ImageView& inputImageView, const glm::uvec2& size, bool rebuildPipeline)
	{
		m_taaPreviousImages[0].reset();
		m_taaPreviousImages[1].reset();
		m_taaPreviousImageViews[0].reset();
		m_taaPreviousImageViews[1].reset();

		// Populate TAA jitter offsets with quasi-random sequence.
		//const float goldenRatio = 1.32471795724474602596f;
		//const glm::vec2 a = glm::vec2(1.0f / goldenRatio, 1.0f / (goldenRatio * goldenRatio));
		for (size_t i = 0; i < m_taaJitterOffsets.size(); ++i)
		{
			//m_taaJitterOffsets[i].x = (fmodf(0.5f + a.x * i, 1.0f) - 0.5f) / size.x;
			//m_taaJitterOffsets[i].y = (fmodf(0.5f + a.y * i, 1.0f) - 0.5f) / size.y;
			m_taaJitterOffsets[i].x = (2.0f * Halton(i + 1, 2) - 1.0f) / size.x;
			m_taaJitterOffsets[i].y = (2.0f * Halton(i + 1, 3) - 1.0f) / size.y;
		}

		if (!CreateTAAImage(device, allocator, size))
		{
			return false;
		}

		std::array<vk::DescriptorImageInfo, 1> linearSamplerInfos =
		{
			vk::DescriptorImageInfo(m_linearSampler->Get(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal)
		};

		std::array<vk::DescriptorImageInfo, 1> nearestSamplerInfos =
		{
			vk::DescriptorImageInfo(m_nearestSampler->Get(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal)
		};

		std::array<vk::DescriptorImageInfo, 1> inputImageInfos =
		{
			 vk::DescriptorImageInfo(nullptr, inputImageView.Get(), vk::ImageLayout::eShaderReadOnlyOptimal)
		};

		std::array<vk::DescriptorImageInfo, 1> prevImageInfos =
		{
			 vk::DescriptorImageInfo(nullptr, m_taaPreviousImageViews[0]->Get(), vk::ImageLayout::eShaderReadOnlyOptimal)
		};

		std::array<vk::DescriptorImageInfo, 1> velocityImageInfos =
		{
			 vk::DescriptorImageInfo(nullptr, m_gBuffer.GetVelocityImageView().Get(), vk::ImageLayout::eShaderReadOnlyOptimal)
		};

		std::array<vk::DescriptorImageInfo, 1> depthImageInfos =
		{
			 vk::DescriptorImageInfo(nullptr, m_gBuffer.GetDepthImageView().Get(), vk::ImageLayout::eShaderReadOnlyOptimal)
		};

		for (uint32_t i = 0; i < m_concurrentFrames; ++i)
		{
			std::array<vk::WriteDescriptorSet, 6> writeDescriptorSets =
			{
				vk::WriteDescriptorSet(m_taaDescriptorSets[i], 0, 0, vk::DescriptorType::eSampler, linearSamplerInfos),
				vk::WriteDescriptorSet(m_taaDescriptorSets[i], 1, 0, vk::DescriptorType::eSampler, nearestSamplerInfos),
				vk::WriteDescriptorSet(m_taaDescriptorSets[i], 2, 0, vk::DescriptorType::eSampledImage, inputImageInfos),
				vk::WriteDescriptorSet(m_taaDescriptorSets[i], 3, 0, vk::DescriptorType::eSampledImage, prevImageInfos),
				vk::WriteDescriptorSet(m_taaDescriptorSets[i], 4, 0, vk::DescriptorType::eSampledImage, velocityImageInfos),
				vk::WriteDescriptorSet(m_taaDescriptorSets[i], 5, 0, vk::DescriptorType::eSampledImage, depthImageInfos)
			};

			device.Get().updateDescriptorSets(writeDescriptorSets, nullptr);
		}

		if (rebuildPipeline)
		{
			std::vector<vk::Format> attachmentFormats = { swapChainFormat, m_taaPreviousImages[0]->GetFormat() };

			std::vector<vk::DescriptorSetLayout> descriptorSetLayouts = { m_taaDescriptorSetLayout.get() };
			std::vector<vk::PushConstantRange> pushConstantRanges = { vk::PushConstantRange(vk::ShaderStageFlagBits::eVertex, 0, sizeof(uint32_t)) };

			PipelineLayout* pipelineLayout = static_cast<PipelineLayout*>(m_taaShader.get());
			if (!pipelineLayout->Rebuild(physicalDevice, device, attachmentFormats, vk::Format::eUndefined, descriptorSetLayouts, pushConstantRanges))
			{
				return false;
			}
		}

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

	bool PostProcessing::SetupTAADescriptorSetLayout(const Device& device)
	{
		std::array<vk::DescriptorSetLayoutBinding, 6> layoutBindings =
		{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eSampler, 1, vk::ShaderStageFlagBits::eFragment),
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eSampler, 1, vk::ShaderStageFlagBits::eFragment),
			vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eSampledImage, 1, vk::ShaderStageFlagBits::eFragment),
			vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eSampledImage, 1, vk::ShaderStageFlagBits::eFragment),
			vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eSampledImage, 1, vk::ShaderStageFlagBits::eFragment),
			vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eSampledImage, 1, vk::ShaderStageFlagBits::eFragment)
		};

		vk::DescriptorSetLayoutCreateInfo layoutInfo(vk::DescriptorSetLayoutCreateFlags(), layoutBindings);

		const vk::Device& deviceImp = device.Get();
		m_taaDescriptorSetLayout = deviceImp.createDescriptorSetLayoutUnique(layoutInfo);
		if (!m_taaDescriptorSetLayout.get())
		{
			Logger::Error("Failed to create descriptor set layout.");
			return false;
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
		const vk::Pipeline& graphicsPipeline = m_taaShader->GetGraphicsPipeline();
		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);

		uint32_t enabled = m_enabled ? 1 : 0;
		commandBuffer.pushConstants(m_taaShader->Get(), vk::ShaderStageFlagBits::eVertex, 0, sizeof(uint32_t), &enabled);
		commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_taaShader->Get(), 0, m_taaDescriptorSets[frameIndex], nullptr);
		commandBuffer.draw(3, 1, 0, 0);
	}
}