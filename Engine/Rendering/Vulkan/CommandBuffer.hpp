#pragma once

#include <vulkan/vulkan.hpp>
#include "../Resources/ICommandBuffer.hpp"

namespace Engine::Rendering::Vulkan
{
	class CommandBuffer : public ICommandBuffer
	{
	public:
		CommandBuffer(const vk::CommandBuffer& commandBuffer);
		const vk::CommandBuffer& Get() const;
		
		virtual void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) const override;

		virtual void BlitImage(const IRenderImage& srcImage, ImageLayout srcLayout,
			const IRenderImage& dstImage, ImageLayout dstLayout, const std::vector<ImageBlit>& regions, Filter filter) const override;

		virtual void PushConstants(const Material* material, ShaderStageFlags stageFlags, uint32_t offset, uint32_t size, uint32_t* value) const override;

		virtual void BindVertexBuffers(uint32_t firstBinding, const std::vector<IBuffer*>& buffers, const std::vector<size_t>& offsets) const override;

		virtual void BindIndexBuffer(const IBuffer& buffer, size_t offset, IndexType indexType) const override;

		virtual void DrawIndexedIndirect(const IBuffer& buffer, size_t offset, uint32_t drawCount, uint32_t stride) const override;

	private:
		const vk::CommandBuffer& m_commandBuffer;
	};
}