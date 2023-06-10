#pragma once

#include <vulkan/vulkan.hpp>
#include <vma/vk_mem_alloc.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Engine::Rendering::Vulkan
{
	class ImageSampler;
	class RenderImage;
	class ImageView;
	class PhysicalDevice;
	class Device;
	class GBuffer;
	class PipelineManager;
	class PipelineLayout;

	class PostProcessing
	{
	public:
		PostProcessing(GBuffer& gBuffer);

		bool Initialise(const PhysicalDevice& physicalDevice, const Device& device,
			const PipelineManager& pipelineManager, VmaAllocator allocator, const glm::uvec2& size);

		bool Rebuild(const PhysicalDevice& physicalDevice, const Device& device, VmaAllocator allocator,
			const glm::uvec2& size);

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
		bool RebuildTAA(const PhysicalDevice& physicalDevice, const Device& device, VmaAllocator allocator,
			const ImageView& inputImageView, const glm::uvec2& size);

		bool CreateTAAImage(const Device& device, VmaAllocator allocator, const glm::uvec2& size);

		bool m_enabled;
		std::unique_ptr<ImageSampler> m_linearSampler;
		std::unique_ptr<ImageSampler> m_nearestSampler;
		GBuffer& m_gBuffer;

		uint32_t m_taaFrameIndex;
		std::array<glm::vec2, 6> m_taaJitterOffsets;
		std::array<std::unique_ptr<RenderImage>, 2> m_taaPreviousImages;
		std::array<std::unique_ptr<ImageView>, 2> m_taaPreviousImageViews;
		PipelineLayout* m_taaShader;
	};
}