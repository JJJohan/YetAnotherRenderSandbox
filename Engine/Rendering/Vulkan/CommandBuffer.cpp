#include "CommandBuffer.hpp"
#include "RenderImage.hpp"
#include "PipelineLayout.hpp"
#include "VulkanTypesInterop.hpp"
#include "Buffer.hpp"
#include "Core/Logger.hpp"
#include "VulkanMemoryBarriers.hpp"

namespace Engine::Rendering::Vulkan
{
	CommandBuffer::CommandBuffer(vk::UniqueCommandBuffer commandBuffer, uint32_t queueFamilyIndex)
		: m_commandBuffer(std::move(commandBuffer))
	{
		m_queueFamilyIndex = queueFamilyIndex;
	}

	const vk::CommandBuffer& CommandBuffer::Get() const
	{
		return m_commandBuffer.get();
	}

	bool CommandBuffer::Begin() const
	{
		vk::CommandBufferBeginInfo beginInfo;
		if (m_commandBuffer->begin(&beginInfo) != vk::Result::eSuccess)
		{
			Logger::Error("Failed to begin recording render command buffer.");
			return false;
		}

		return true;
	}

	void CommandBuffer::BeginRendering(const std::vector<AttachmentInfo>& attachments,
		const std::optional<AttachmentInfo>& depthAttachment, const glm::uvec2& size, uint32_t layerCount) const
	{
		vk::RenderingInfo renderingInfo{};
		renderingInfo.renderArea.offset = vk::Offset2D();
		renderingInfo.renderArea.extent = vk::Extent2D(size.x, size.y);
		renderingInfo.layerCount = layerCount;

		std::vector<vk::RenderingAttachmentInfo> vkColorAttachments(attachments.size());
		for (size_t i = 0; i < attachments.size(); ++i)
			vkColorAttachments[i] = GetAttachmentInfo(attachments[i]);

		renderingInfo.colorAttachmentCount = static_cast<uint32_t>(attachments.size());
		renderingInfo.pColorAttachments = vkColorAttachments.data();

		if (depthAttachment.has_value())
		{
			vk::RenderingAttachmentInfo vkDepthAttachment = GetAttachmentInfo(*depthAttachment);
			renderingInfo.pDepthAttachment = &vkDepthAttachment;
		}

		m_commandBuffer->beginRendering(renderingInfo);

		float width = static_cast<float>(size.x);
		float height = static_cast<float>(size.y);
		vk::Viewport viewport(0.0f, 0.0f, width, height, 0.0f, 1.0f);
		m_commandBuffer->setViewport(0, 1, &viewport);

		vk::Rect2D scissor(renderingInfo.renderArea.offset, renderingInfo.renderArea.extent);
		m_commandBuffer->setScissor(0, 1, &scissor);
	}

	void CommandBuffer::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) const
	{
		m_commandBuffer->draw(vertexCount, instanceCount, firstVertex, firstInstance);
	}

	void CommandBuffer::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) const
	{
		m_commandBuffer->dispatch(groupCountX, groupCountY, groupCountZ);
	}

	void CommandBuffer::BlitImage(const IRenderImage& srcImage, const IRenderImage& dstImage,
		const std::vector<ImageBlit>& regions, Filter filter) const
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

		m_commandBuffer->blitImage(vkSrcImage.Get(), vk::ImageLayout::eTransferSrcOptimal, vkDstImage.Get(), vk::ImageLayout::eTransferDstOptimal, vkRegions, GetFilter(filter));
	}

	void CommandBuffer::PushConstants(const Material* material, ShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const uint32_t* value) const
	{
		const PipelineLayout* pipelineLayout = static_cast<const PipelineLayout*>(material);
		m_commandBuffer->pushConstants(pipelineLayout->Get(), static_cast<vk::ShaderStageFlagBits>(stageFlags), offset, size, value);
	}

	void CommandBuffer::BindVertexBuffers(uint32_t firstBinding, const std::vector<IBuffer*>& buffers, const std::vector<size_t>& offsets) const
	{
		std::vector<vk::Buffer> bufferViews(buffers.size());
		for (size_t i = 0; i < buffers.size(); ++i)
		{
			const Buffer* buffer = static_cast<const Buffer*>(buffers[i]);
			bufferViews[i] = buffer->Get();
		}

		m_commandBuffer->bindVertexBuffers(0, bufferViews, offsets);
	}

	void CommandBuffer::BindIndexBuffer(const IBuffer& buffer, size_t offset, IndexType indexType) const
	{
		m_commandBuffer->bindIndexBuffer(static_cast<const Buffer&>(buffer).Get(), offset, static_cast<vk::IndexType>(indexType));
	}

	void CommandBuffer::DrawIndexedIndirect(const IBuffer& buffer, size_t offset, uint32_t drawCount, uint32_t stride) const
	{
		m_commandBuffer->drawIndexedIndirect(static_cast<const Buffer&>(buffer).Get(), offset, drawCount, stride);
	}

	void CommandBuffer::DrawIndexedIndirectCount(const IBuffer& buffer, size_t offset, const IBuffer& countBuffer, size_t countOffset, uint32_t maxDrawCount, uint32_t stride) const
	{
		m_commandBuffer->drawIndexedIndirectCount(static_cast<const Buffer&>(buffer).Get(), offset, static_cast<const Buffer&>(buffer).Get(), countOffset, maxDrawCount, stride);
	}

	void CommandBuffer::MemoryBarrier(MaterialStageFlags srcStage, MaterialAccessFlags srcMask, MaterialStageFlags dstStage, MaterialAccessFlags dstMask) const
	{
		vk::PipelineStageFlagBits2 vkSrcStage = static_cast<vk::PipelineStageFlagBits2>(srcStage);
		vk::PipelineStageFlagBits2 vkDstStage = static_cast<vk::PipelineStageFlagBits2>(dstStage);
		vk::AccessFlagBits2 vkSrcMask = static_cast<vk::AccessFlagBits2>(srcMask);
		vk::AccessFlagBits2 vkDstMask = static_cast<vk::AccessFlagBits2>(dstMask);

		vk::MemoryBarrier2 barrier(vkSrcStage, vkSrcMask, vkDstStage, vkDstMask);
		vk::DependencyInfo dependencyInfo(vk::DependencyFlagBits::eByRegion, 1, &barrier);
		m_commandBuffer->pipelineBarrier2(dependencyInfo);
	}

	void CommandBuffer::MemoryBarrier(const IMemoryBarriers& memoryBarriersContainer) const
	{
		const VulkanMemoryBarriers& vulkanMemoryBarriers = static_cast<const VulkanMemoryBarriers&>(memoryBarriersContainer);
		const std::vector<vk::MemoryBarrier2>& memoryBarriers = vulkanMemoryBarriers.GetMemoryBarriers();
		const std::vector<vk::BufferMemoryBarrier2>& bufferMemoryBarriers = vulkanMemoryBarriers.GetBufferMemoryBarriers();
		const std::vector<vk::ImageMemoryBarrier2>& imageMemoryBarriers = vulkanMemoryBarriers.GetImageMemoryBarriers();

		vk::DependencyInfo dependencyInfo(vk::DependencyFlagBits::eByRegion,
			static_cast<uint32_t>(memoryBarriers.size()), memoryBarriers.data(),
			static_cast<uint32_t>(bufferMemoryBarriers.size()), bufferMemoryBarriers.data(),
			static_cast<uint32_t>(imageMemoryBarriers.size()), imageMemoryBarriers.data());
		m_commandBuffer->pipelineBarrier2(dependencyInfo);
	}

	void CommandBuffer::ClearColourImage(const IRenderImage& image, const Colour& colour) const
	{
		vk::ImageSubresourceRange subResourceRange(vk::ImageAspectFlagBits::eColor, 0, image.GetMipLevels(), 0, image.GetLayerCount());
		m_commandBuffer->clearColorImage(static_cast<const RenderImage&>(image).Get(),
			GetImageLayout(image.GetLayout()), vk::ClearColorValue(colour, colour, colour, colour), subResourceRange);
	}

	void CommandBuffer::ClearDepthStencilImage(const IRenderImage& image, float depth, uint32_t stencil) const
	{
		vk::ImageSubresourceRange subResourceRange(vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil, 0, image.GetMipLevels(), 0, image.GetLayerCount());
		m_commandBuffer->clearDepthStencilImage(static_cast<const RenderImage&>(image).Get(),
			GetImageLayout(image.GetLayout()), vk::ClearDepthStencilValue(depth, stencil), subResourceRange);
	}

	void CommandBuffer::FillBuffer(const IBuffer& buffer, size_t offset, size_t size, uint32_t data) const
	{
		m_commandBuffer->fillBuffer(static_cast<const Buffer&>(buffer).Get(), offset, size, data);
	}
}