#include "CommandBuffer.hpp"
#include "RenderImage.hpp"
#include "PipelineLayout.hpp"
#include "VulkanTypesInterop.hpp"
#include "Buffer.hpp"

namespace Engine::Rendering::Vulkan
{
	CommandBuffer::CommandBuffer(const vk::CommandBuffer& commandBuffer)
		: m_commandBuffer(commandBuffer)
	{
	}

	const vk::CommandBuffer& CommandBuffer::Get() const
	{
		return m_commandBuffer;
	}

	void CommandBuffer::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) const
	{
		m_commandBuffer.draw(vertexCount, instanceCount, firstVertex, firstInstance);
	}

	void CommandBuffer::BlitImage(const IRenderImage& srcImage, ImageLayout srcLayout,
		const IRenderImage& dstImage, ImageLayout dstLayout, const std::vector<ImageBlit>& regions, Filter filter) const
	{
		const RenderImage& vkSrcImage = static_cast<const RenderImage&>(srcImage);
		const RenderImage& vkDstImage = static_cast<const RenderImage&>(dstImage);

		std::vector<vk::ImageBlit> vkRegions(regions.size());
		for (size_t i = 0; i < regions.size(); ++i)
		{
			const ImageBlit& region = regions[i];

			std::array<vk::Offset3D, 2U> srcOffsets;
			for (size_t j = 0; j < srcOffsets.size(); ++j)
			{
				const glm::uvec3& srcOffset = region.srcOffsets[j];
				srcOffsets[j] = vk::Offset3D(srcOffset.x, srcOffset.y, srcOffset.z);
			}
			
			std::array<vk::Offset3D, 2U> dstOffsets;
			for (size_t j = 0; j < dstOffsets.size(); ++j)
			{
				const glm::uvec3& dstOffset = region.dstOffsets[j];
				dstOffsets[j] = vk::Offset3D(dstOffset.x, dstOffset.y, dstOffset.z);
			}

			vkRegions[i] = vk::ImageBlit(
				vk::ImageSubresourceLayers(GetImageAspectFlags(region.srcSubresource.aspectFlags), region.srcSubresource.mipLevel, region.srcSubresource.baseArrayLayer, region.srcSubresource.layerCount),
				srcOffsets,
				vk::ImageSubresourceLayers(GetImageAspectFlags(region.dstSubresource.aspectFlags), region.dstSubresource.mipLevel, region.dstSubresource.baseArrayLayer, region.dstSubresource.layerCount),
				dstOffsets);
		}

		m_commandBuffer.blitImage(vkSrcImage.Get(), GetImageLayout(srcLayout), vkDstImage.Get(), GetImageLayout(dstLayout), vkRegions, GetFilter(filter));
	}

	void CommandBuffer::PushConstants(const Material* material, ShaderStageFlags stageFlags, uint32_t offset, uint32_t size, uint32_t* value) const
	{
		const PipelineLayout* pipelineLayout = static_cast<const PipelineLayout*>(material);
		m_commandBuffer.pushConstants(pipelineLayout->Get(), static_cast<vk::ShaderStageFlagBits>(stageFlags), offset, size, value);
	}

	void CommandBuffer::BindVertexBuffers(uint32_t firstBinding, const std::vector<IBuffer*>& buffers, const std::vector<size_t>& offsets) const
	{
		std::vector<vk::Buffer> bufferViews(buffers.size());
		for (size_t i = 0; i < buffers.size(); ++i)
		{
			const Buffer* buffer = static_cast<const Buffer*>(buffers[i]);
			bufferViews[i] = buffer->Get();
		}

		m_commandBuffer.bindVertexBuffers(0, bufferViews, offsets);
	}

	void CommandBuffer::BindIndexBuffer(const IBuffer& buffer, size_t offset, IndexType indexType) const
	{
		m_commandBuffer.bindIndexBuffer(static_cast<const Buffer&>(buffer).Get(), offset, static_cast<vk::IndexType>(indexType));
	}

	void CommandBuffer::DrawIndexedIndirect(const IBuffer& buffer, size_t offset, uint32_t drawCount, uint32_t stride) const
	{
		m_commandBuffer.drawIndexedIndirect(static_cast<const Buffer&>(buffer).Get(), offset, drawCount, stride);
	}
}