#pragma once

#include <vulkan/vulkan.hpp>
#include <vma/vk_mem_alloc.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Engine::Rendering::Vulkan
{
	class ImageSampler;
	class DescriptorPool;
	class PipelineLayout;
	class RenderImage;
	class ImageView;
	class PhysicalDevice;
	class Device;
	class GBuffer;

	class PostProcessing
	{
	public:
		PostProcessing(GBuffer& gBuffer, uint32_t concurrentFrames);

		bool Initialise(const PhysicalDevice& physicalDevice, const Device& device,
			VmaAllocator allocator, vk::Format swapChainFormat, const glm::uvec2& size);

		bool Rebuild(const PhysicalDevice& physicalDevice, const Device& device, VmaAllocator allocator,
			vk::Format swapChainFormat, const glm::uvec2& size, bool rebuildPipeline);

		void TransitionTAAImageLayouts(const Device& device, const vk::CommandBuffer& commandBuffer) const;
		void BlitTAA(const Device& device, const vk::CommandBuffer& commandBuffer) const;

		void Draw(const vk::CommandBuffer& commandBuffer, uint32_t frameIndex) const;

		inline const glm::vec2& GetTAAJitter(const glm::vec2& size)
		{
			return m_taaJitterOffsets[++m_taaFrameIndex % m_taaJitterOffsets.size()];
		}

		inline const ImageView& GetTAAPrevImageView() const { return *m_taaPreviousImageViews[1]; };

		inline void SetEnabled(bool enabled) { m_enabled = enabled; };
		inline bool IsEnabled() { return m_enabled; };

	private:
		bool InitialiseTAA(const PhysicalDevice& physicalDevice, const Device& device,
			VmaAllocator allocator, vk::Format swapChainFormat, const glm::uvec2& size);

		bool RebuildTAA(const PhysicalDevice& physicalDevice, const Device& device, VmaAllocator allocator,
			vk::Format swapChainFormat, const ImageView& inputImageView, const glm::uvec2& size, bool rebuildPipeline);

		bool CreateTAAImage(const Device& device, VmaAllocator allocator, const glm::uvec2& size);

		bool SetupTAADescriptorSetLayout(const Device& device);

		bool m_enabled;
		const uint32_t m_concurrentFrames;
		std::unique_ptr<ImageSampler> m_linearSampler;
		std::unique_ptr<ImageSampler> m_nearestSampler;
		GBuffer& m_gBuffer;

		uint32_t m_taaFrameIndex;
		std::array<glm::vec2, 6> m_taaJitterOffsets;
		std::unique_ptr<PipelineLayout> m_taaShader;
		std::array<std::unique_ptr<RenderImage>, 2> m_taaPreviousImages;
		std::array<std::unique_ptr<ImageView>, 2> m_taaPreviousImageViews;
		std::vector<vk::DescriptorSet> m_taaDescriptorSets;
		vk::UniqueDescriptorSetLayout m_taaDescriptorSetLayout;
		std::unique_ptr<DescriptorPool> m_taaDescriptorPool;
	};
}