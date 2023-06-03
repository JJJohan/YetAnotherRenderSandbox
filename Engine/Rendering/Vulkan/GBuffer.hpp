#pragma once

#include <vulkan/vulkan.hpp>
#include <vma/vk_mem_alloc.h>
#include <glm/glm.hpp>
#include "RenderImage.hpp"
#include "ImageView.hpp"

#define GBUFFER_SIZE 5 // albedo, normal, worldPos, metalRoughness, velocity

namespace Engine::Rendering::Vulkan
{
	class Device;
	class PhysicalDevice;
	class ImageView;
	class RenderImage;
	class DescriptorPool;
	class PipelineLayout;
	class ImageSampler;
	class Buffer;
	class ShadowMap;

	class GBuffer
	{
	public:
		GBuffer(uint32_t concurrentFrames);

		bool Initialise(const PhysicalDevice& physicalDevice, const Device& device, VmaAllocator allocator,
			vk::Format depthFormat, const glm::uvec2& size, const std::vector<std::unique_ptr<Buffer>>& frameInfoBuffers,
			const std::vector<std::unique_ptr<Buffer>>& lightBuffers, const ShadowMap& shadowMap);
		bool Rebuild(const PhysicalDevice& physicalDevice, const Device& device, VmaAllocator allocator,
			const glm::uvec2& size, const std::vector<std::unique_ptr<Buffer>>& frameInfoBuffers,
			const std::vector<std::unique_ptr<Buffer>>& lightBuffers, const ShadowMap& shadowMap, bool rebuildPipeline);
		void TransitionImageLayouts(const Device& device, const vk::CommandBuffer& commandBuffer, vk::ImageLayout newLayout);
		void TransitionDepthLayout(const Device& device, const vk::CommandBuffer& commandBuffer, vk::ImageLayout newLayout);
		void DrawFinalImage(const vk::CommandBuffer& commandBuffer, uint32_t currentFrameIndex) const;
		uint64_t GetMemoryUsage() const;

		std::vector<vk::RenderingAttachmentInfo> GetRenderAttachments() const;
		vk::RenderingAttachmentInfo GetDepthAttachment() const;
		inline const ImageView& GetVelocityImageView() const { return *m_gBufferImageViews[4]; }
		inline const std::vector<vk::Format>& GetImageFormats() const { return m_imageFormats; }
		inline vk::Format GetDepthFormat() const { return m_depthFormat; }
		inline RenderImage& GetOutputImage() const { return *m_outputImage; }
		inline const ImageView& GetOutputImageView() const { return *m_outputImageView; }
		inline const ImageView& GetDepthImageView() const { return *m_depthImageView; }

	private:
		bool CreateImageAndView(const Device& device, VmaAllocator allocator, const glm::uvec2& size, vk::Format format);
		bool CreateColorImages(const Device& device, VmaAllocator allocator, const glm::uvec2& size);
		bool CreateDepthImage(const Device& device, VmaAllocator allocator, const glm::uvec2& size);
		bool CreateOutputImage(const Device& device, VmaAllocator allocator, const glm::uvec2& size);
		bool SetupDescriptorSetLayout(const Device& device, uint32_t shadowCascades);

		const uint32_t m_concurrentFrames;
		std::unique_ptr<PipelineLayout> m_combineShader;
		std::vector<std::unique_ptr<RenderImage>> m_gBufferImages;
		std::vector<std::unique_ptr<ImageView>> m_gBufferImageViews;
		std::vector<vk::Format> m_imageFormats;
		std::unique_ptr<RenderImage> m_depthImage;
		std::unique_ptr<ImageView> m_depthImageView;
		std::unique_ptr<RenderImage> m_outputImage;
		std::unique_ptr<ImageView> m_outputImageView;
		std::vector<vk::DescriptorSet> m_descriptorSets;
		vk::UniqueDescriptorSetLayout m_descriptorSetLayout;
		std::unique_ptr<DescriptorPool> m_descriptorPool;
		std::unique_ptr<ImageSampler> m_sampler;
		std::unique_ptr<ImageSampler> m_shadowSampler;
		vk::Format m_depthFormat;
	};
}