#pragma once

#include "../Types.hpp"
#include <glm/glm.hpp>
#include <array>
#include "AttachmentInfo.hpp"
#include <optional>

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

		inline virtual void Reset() const = 0;

		inline virtual bool Begin() const = 0;

		inline virtual void End() const = 0;

		virtual void BeginRendering(const std::vector<AttachmentInfo>& attachments, 
			const std::optional<AttachmentInfo>& depthAttachment, const glm::uvec2& size,
			uint32_t layerCount) const = 0;

		inline virtual void EndRendering() const = 0;

		virtual void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) const = 0;

		virtual void BlitImage(const IRenderImage& srcImage, const IRenderImage& dstImage, const std::vector<ImageBlit>& regions, Filter filter) const = 0;

		virtual void PushConstants(const Material* material, ShaderStageFlags stageFlags, uint32_t offset, uint32_t size, uint32_t* value) const = 0;

		virtual void BindVertexBuffers(uint32_t firstBinding, const std::vector<IBuffer*>& buffers, const std::vector<size_t>& offsets) const = 0;

		virtual void BindIndexBuffer(const IBuffer& buffer, size_t offset, IndexType indexType) const = 0;

		virtual void DrawIndexedIndirect(const IBuffer& buffer, size_t offset, uint32_t drawCount, uint32_t stride) const = 0;
	};
}