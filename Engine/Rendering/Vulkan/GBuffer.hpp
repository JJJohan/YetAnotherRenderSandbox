#pragma once

#include <vulkan/vulkan.hpp>
#include <vma/vk_mem_alloc.h>
#include <glm/glm.hpp>

#define GBUFFER_SIZE 4 // albedo, normal, worldPos, metalRoughness

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

	class GBuffer
	{
	public:
		GBuffer(uint32_t concurrentFrames);

		bool Initialise(const PhysicalDevice& physicalDevice, const Device& device,
			VmaAllocator allocator, vk::Format swapChainFormat, const std::vector<std::unique_ptr<Buffer>>& frameInfoBuffers,
			const glm::uvec2& size);
		bool Rebuild(const PhysicalDevice& physicalDevice, const Device& device, VmaAllocator allocator,
			const glm::uvec2& size, vk::Format swapChainFormat, const std::vector<std::unique_ptr<Buffer>>& frameInfoBuffers,
			bool rebuildPipeline);
		void TransitionImageLayouts(const Device& device, const vk::CommandBuffer& commandBuffer, vk::ImageLayout newLayout);
		void TransitionDepthLayout(const Device& device, const vk::CommandBuffer& commandBuffer, vk::ImageLayout newLayout);
		void DrawFinalImage(const vk::CommandBuffer& commandBuffer, uint32_t currentFrameIndex) const;

		std::vector<vk::RenderingAttachmentInfo> GetRenderAttachments() const;
		vk::RenderingAttachmentInfo GetDepthAttachment() const;
		std::vector<vk::ImageView> GetImageViews() const;
		const std::vector<vk::Format>& GetImageFormats() const;
		vk::Format GetDepthFormat() const;

		RenderImage& GetShadowImage() const;
		const ImageView& GetShadowImageView() const;
		vk::RenderingAttachmentInfo GetShadowAttachment() const;

	private:
		bool CreateImageAndView(const Device& device, VmaAllocator allocator, const glm::uvec2& size, vk::Format format);
		bool CreateColorImages(const Device& device, VmaAllocator allocator, const glm::uvec2& size);
		bool CreateDepthImage(const PhysicalDevice& physicalDevice, const Device& device, VmaAllocator allocator, const glm::uvec2& size);
		bool CreateShadowImage(const Device& device, VmaAllocator allocator);
		bool SetupDescriptorSetLayout(const Device& device);

		const uint32_t m_concurrentFrames;
		std::unique_ptr<PipelineLayout> m_combineShader;
		std::vector<std::unique_ptr<RenderImage>> m_gBufferImages;
		std::vector<std::unique_ptr<ImageView>> m_gBufferImageViews;
		std::vector<vk::Format> m_imageFormats;
		std::unique_ptr<RenderImage> m_depthImage;
		std::unique_ptr<ImageView> m_depthImageView;
		std::vector<vk::DescriptorSet> m_descriptorSets;
		vk::UniqueDescriptorSetLayout m_descriptorSetLayout;
		std::unique_ptr<DescriptorPool> m_descriptorPool;
		std::unique_ptr<ImageSampler> m_sampler;
		std::unique_ptr<ImageSampler> m_shadowSampler;
		std::unique_ptr<RenderImage> m_shadowImage;
		std::unique_ptr<ImageView> m_shadowImageView;
		vk::Format m_depthFormat;
	};
}