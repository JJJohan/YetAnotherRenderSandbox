#pragma once

#include <vulkan/vulkan.hpp>
#include "../SceneManager.hpp"
#include <memory>

struct VmaAllocator_T;
typedef struct VmaAllocator_T* VmaAllocator;

namespace Engine
{
	class Image;
	class ChunkData;
	class AsyncData;
}

namespace Engine::Rendering
{
	class Camera;
}

namespace Engine::Rendering::Vulkan
{
	class Buffer;
	class Device;
	class PhysicalDevice;
	class CommandBuffer;
	class DescriptorPool;
	class RenderImage;
	class ImageView;
	class ImageSampler;
	class VulkanRenderer;
	class PipelineLayout;

	class VulkanSceneManager : public SceneManager
	{
	public:
		VulkanSceneManager(VulkanRenderer& renderer);

		bool Initialise(const std::vector<vk::Format>& gBufferFormats, vk::Format depthFormat);

		virtual bool Build(ChunkData* chunkData, AsyncData& asyncData) override;

		void Draw(const vk::CommandBuffer& commandBuffer, uint32_t currentFrameIndex);
		void DrawShadows(const vk::CommandBuffer& commandBuffer, uint32_t currentFrameIndex);

		bool RebuildShader(const Device& device, const std::vector<vk::Format>& gBufferFormats, vk::Format depthFormat);

	private:
		bool SetupPBRDescriptorSetLayout(const Device& device, uint32_t imageCount);
		bool InitPBRShaderResources(const Device& device, uint32_t concurrentFrames, const std::vector<vk::Format>& gBufferFormats, vk::Format depthFormat, VmaAllocator allocator);
		bool InitShadowShaderResources(const Device& device, uint32_t concurrentFrames, vk::Format depthFormat, VmaAllocator allocator);

		bool SetupIndirectDrawBuffer(const Device& device, const vk::CommandBuffer& commandBuffer, ChunkData* chunkData,
			std::vector<std::unique_ptr<Buffer>>& temporaryBuffers, VmaAllocator allocator);

		bool SetupVertexBuffers(const Device& device, const vk::CommandBuffer& commandBuffer, ChunkData* chunkData,
			std::vector<std::unique_ptr<Buffer>>& temporaryBuffers, VmaAllocator allocator);

		bool SetupIndexBuffer(const Device& device, const vk::CommandBuffer& commandBuffer, ChunkData* chunkData,
			std::vector<std::unique_ptr<Buffer>>& temporaryBuffers, VmaAllocator allocator);

		bool SetupRenderImage(AsyncData* asyncData, const Device& device, const PhysicalDevice& physicalDevice, const vk::CommandBuffer& commandBuffer, ChunkData* chunkData,
			std::vector<std::unique_ptr<Buffer>>& temporaryBuffers, VmaAllocator allocator, float maxAnisotropy, uint32_t& imageCount);

		bool SetupMeshInfoBuffer(const Device& device, const vk::CommandBuffer& commandBuffer, ChunkData* chunkData,
			std::vector<std::unique_ptr<Buffer>>& temporaryBuffers, VmaAllocator allocator);

		bool CreateStagingBuffer(VmaAllocator allocator, const Device& device,
			const vk::CommandBuffer& commandBuffer, const Buffer* destinationBuffer, const void* data,
			uint64_t size, std::vector<std::unique_ptr<Buffer>>& copyBufferCollection);

		bool CreateImageStagingBuffer(VmaAllocator allocator, const Device& device,
			const vk::CommandBuffer& commandBufferl, const RenderImage* destinationImage, uint32_t mipLevel, const void* data, uint64_t size,
			std::vector<std::unique_ptr<Buffer>>& copyBufferCollection);

		VulkanRenderer& m_renderer;

		std::vector<vk::Format> m_lastKnownColorFormats;
		vk::Format m_lastKnownDepthFormat;

		std::shared_ptr<RenderImage> m_blankImage;
		std::shared_ptr<ImageView> m_blankImageView;
		std::unique_ptr<ImageSampler> m_sampler;

		std::unique_ptr<Buffer> m_indirectDrawBuffer;
		std::vector<std::unique_ptr<Buffer>> m_vertexBuffers;
		std::unique_ptr<Buffer> m_indexBuffer;
		std::unique_ptr<Buffer> m_meshInfoBuffer;
		std::vector<std::unique_ptr<RenderImage>> m_imageArray;
		std::vector<std::unique_ptr<ImageView>> m_imageArrayView;

		std::vector<uint32_t> m_vertexOffsets;
		std::vector<uint32_t> m_indexOffsets;
		std::vector<uint32_t> m_indexCounts;

		std::vector<vk::DrawIndexedIndirectCommand> m_indirectDrawCommands;

		vk::UniqueDescriptorSetLayout m_pbrDescriptorSetLayout;
		std::unique_ptr<DescriptorPool> m_pbrDescriptorPool;
		std::vector<vk::DescriptorSet> m_pbrDescriptorSets;
		std::unique_ptr<PipelineLayout> m_pbrShader;

		vk::UniqueDescriptorSetLayout m_shadowDescriptorSetLayout;
		std::unique_ptr<DescriptorPool> m_shadowDescriptorPool;
		std::vector<vk::DescriptorSet> m_shadowDescriptorSets;
		std::unique_ptr<PipelineLayout> m_shadowShader;
	};
}