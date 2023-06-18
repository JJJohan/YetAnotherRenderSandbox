#pragma once

#include "../Types.hpp"
#include <glm/glm.hpp>
#include <array>

namespace Engine::Rendering
{
	struct ImageSubresourceLayers
	{
		ImageAspectFlags aspectFlags;
		uint32_t mipLevel;
		uint32_t baseArrayLayer;
		uint32_t layerCount;
	};

	struct ImageBlit
	{
		ImageSubresourceLayers srcSubresource;
		std::array<glm::uvec3, 2> srcOffsets;
		ImageSubresourceLayers dstSubresource;
		std::array<glm::uvec3, 2> dstOffsets;
	};

	class IRenderImage;
	class Material;
	class IBuffer;

	class ICommandBuffer
	{
	public:
		ICommandBuffer() = default;
		virtual ~ICommandBuffer() = default;

		virtual void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) const = 0;

		virtual void BlitImage(const IRenderImage& srcImage, ImageLayout srcLayout,
			const IRenderImage& dstImage, ImageLayout dstLayout, const std::vector<ImageBlit>& regions, Filter filter) const = 0;

		virtual void PushConstants(const Material* material, ShaderStageFlags stageFlags, uint32_t offset, uint32_t size, uint32_t* value) const = 0;

		virtual void BindVertexBuffers(uint32_t firstBinding, const std::vector<IBuffer*>& buffers, const std::vector<size_t>& offsets) const = 0;

		virtual void BindIndexBuffer(const IBuffer& buffer, size_t offset, IndexType indexType) const = 0;

		virtual void DrawIndexedIndirect(const IBuffer& buffer, size_t offset, uint32_t drawCount, uint32_t stride) const = 0;
	};
}