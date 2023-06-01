#pragma once

#include <vulkan/vulkan.hpp>
#include <vma/vk_mem_alloc.h>
#include <glm/glm.hpp>

namespace Engine::Rendering
{
	class Camera;
}

namespace Engine::Rendering::Vulkan
{
	class Device;
	class ImageView;
	class RenderImage;
	class Buffer;

	struct ShadowCascadeData
	{
		const std::vector<float>& CascadeSplits;
		const std::vector<glm::mat4>& CascadeMatrices;

		ShadowCascadeData(const std::vector<float>& splits, const std::vector<glm::mat4>& matrices)
			: CascadeSplits(splits)
			, CascadeMatrices(matrices)
		{
		}
	};

	class ShadowMap
	{
	public:
		ShadowMap();
		bool Rebuild(const Device& device, VmaAllocator allocator, vk::Format depthFormat);
		ShadowCascadeData UpdateCascades(const Camera& camera, const glm::vec3& lightDir);
		RenderImage& GetShadowImage(uint32_t index) const;
		const ImageView& GetShadowImageView(uint32_t index) const;
		vk::RenderingAttachmentInfo GetShadowAttachment(uint32_t index) const;
		ShadowCascadeData GetShadowCascadeData() const;
		uint32_t GetCascadeCount() const;
		uint64_t GetMemoryUsage() const;

	private:
		bool CreateShadowImages(const Device& device, VmaAllocator allocator, vk::Format depthFormat);

		uint32_t m_cascadeCount;
		std::vector<float> m_cascadeSplits;
		std::vector<glm::mat4> m_cascadeMatrices;
		std::vector<std::unique_ptr<RenderImage>> m_shadowImages;
		std::vector<std::unique_ptr<ImageView>> m_shadowImageViews;
	};
}