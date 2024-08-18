#pragma once

#include <vulkan/vulkan.hpp>
#include "../Resources/ICommandBuffer.hpp"

namespace Engine::Rendering::Vulkan
{
	class CommandBuffer : public ICommandBuffer
	{
	public:
		CommandBuffer(vk::UniqueCommandBuffer commandBuffer, uint32_t queueFamilyIndex);
		const vk::CommandBuffer& Get() const;

		inline virtual void Reset() const override
		{
			m_commandBuffer->reset();
		}

		inline virtual bool Begin() const;

		inline virtual void End() const override
		{
			m_commandBuffer->end();
		}

		virtual void BeginRendering(const std::vector<AttachmentInfo>& attachments,
			const std::optional<AttachmentInfo>& depthAttachment, const glm::uvec2& size,
			uint32_t layerCount) const override;

		inline virtual void EndRendering() const override
		{
			m_commandBuffer->endRendering();
		}

		virtual void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) const override;

		virtual void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) const override;

		virtual void BlitImage(const IRenderImage& srcImage, const IRenderImage& dstImage, const std::vector<ImageBlit>& regions, Filter filter) const override;

		virtual void PushConstants(const Material* material, ShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const uint32_t* value) const override;

		virtual void BindVertexBuffers(uint32_t firstBinding, const std::vector<IBuffer*>& buffers, const std::vector<size_t>& offsets) const override;

		virtual void BindIndexBuffer(const IBuffer& buffer, size_t offset, IndexType indexType) const override;

		virtual void DrawIndexedIndirect(const IBuffer& buffer, size_t offset, uint32_t drawCount, uint32_t stride) const override;

		virtual void DrawIndexedIndirectCount(const IBuffer& buffer, size_t offset, const IBuffer& countBuffer, size_t countOffset, uint32_t maxDrawCount, uint32_t stride) const override;

		virtual void MemoryBarrier(MaterialStageFlags srcStage, MaterialAccessFlags srcMask, MaterialStageFlags dstStage, MaterialAccessFlags dstMask) const override;

		virtual void MemoryBarrier(const IMemoryBarriers& memoryBarriersContainer) const override;

		virtual void FillBuffer(const IBuffer& buffer, size_t offset, size_t size, uint32_t data) const override;

	private:
		vk::UniqueCommandBuffer m_commandBuffer;
	};
}