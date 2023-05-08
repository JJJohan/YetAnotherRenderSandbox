#pragma once

#include <vulkan/vulkan.hpp>
#include "../SceneManager.hpp"
#include <memory>

struct VmaAllocator_T;
typedef struct VmaAllocator_T* VmaAllocator;

namespace Engine
{
	class Image;
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
	class PipelineLayout;
	class CommandPool;
	class CommandBuffer;
	class DescriptorPool;
	class RenderImage;
	class ImageView;
	class ImageSampler;
	class RenderPass;

	class VulkanSceneManager : public SceneManager
	{
	public:
		VulkanSceneManager(const uint32_t maxConcurrentFrames);

		bool Initialise(VmaAllocator allocator, const Device& device, const CommandPool& resourceCommandPool,
			Shader* shader, const vk::PhysicalDeviceLimits& limits);

		bool Update(VmaAllocator allocator, const Device& device, const CommandPool& resourceCommandPool,
			const std::vector<std::unique_ptr<Buffer>>& frameInfoBuffers, const RenderPass& renderPass, const vk::PhysicalDeviceLimits& limits);

		void Draw(const vk::CommandBuffer& commandBuffer, uint32_t currentFrameIndex);

	private:
		bool SetupIndirectDrawBuffer(const Device& device, const CommandPool& resourceCommandPool,
			std::vector<vk::UniqueCommandBuffer>& resourceCommands, std::vector<std::unique_ptr<Buffer>>& temporaryBuffers,
			VmaAllocator allocator);

		bool SetupVertexBuffers(const Device& device, const CommandPool& resourceCommandPool,
			std::vector<vk::UniqueCommandBuffer>& resourceCommands, std::vector<std::unique_ptr<Buffer>>& temporaryBuffers,
			VmaAllocator allocator);

		bool SetupIndexBuffer(const Device& device, const CommandPool& resourceCommandPool,
			std::vector<vk::UniqueCommandBuffer>& resourceCommands, std::vector<std::unique_ptr<Buffer>>& temporaryBuffers,
			VmaAllocator allocator);

		bool SetupRenderImage(const Device& device, const CommandPool& resourceCommandPool,
			std::vector<vk::UniqueCommandBuffer>& resourceCommands, std::vector<std::unique_ptr<Buffer>>& temporaryBuffers,
			VmaAllocator allocator, float maxAnisotropy, uint32_t& imageCount);

		bool SetupMeshInfoBuffer(const Device& device, const CommandPool& resourceCommandPool,
			std::vector<vk::UniqueCommandBuffer>& resourceCommands, std::vector<std::unique_ptr<Buffer>>& temporaryBuffers,
			VmaAllocator allocator, vk::DeviceSize minOffsetAlignment);

		bool CreateStagingBuffer(VmaAllocator allocator, const Device& device,
			const CommandPool& resourceCommandPool, const Buffer* destinationBuffer, const void* data,
			uint64_t size, std::vector<std::unique_ptr<Buffer>>& copyBufferCollection,
			std::vector<vk::UniqueCommandBuffer>& copyCommandCollection);

		bool CreateImageStagingBuffer(VmaAllocator allocator, const Device& device,
			const CommandPool& resourceCommandPool, const RenderImage* destinationImage, const void* data, uint64_t size,
			std::vector<std::unique_ptr<Buffer>>& copyBufferCollection,
			std::vector<vk::UniqueCommandBuffer>& copyCommandCollection);

		const uint32_t m_maxConcurrentFrames;

		vk::UniqueFence m_resourceFence;
		std::vector<vk::UniqueCommandBuffer> m_prevFrameCommandBuffers;
		std::vector<std::unique_ptr<Buffer>> m_prevFrameBuffers;

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

		std::unique_ptr<DescriptorPool> m_descriptorPool;
		std::vector<vk::DescriptorSet> m_descriptorSets;
	};
}