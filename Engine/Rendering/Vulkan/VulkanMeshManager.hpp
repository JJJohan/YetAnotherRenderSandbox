#pragma once

#include <vulkan/vulkan.hpp>
#include "../MeshManager.hpp"
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

	class VulkanMeshManager : public MeshManager
	{
	public:
		VulkanMeshManager(const uint32_t maxConcurrentFrames);

		virtual void DestroyMesh(uint32_t id);

		bool Update(VmaAllocator allocator, const Device& device, const CommandPool& resourceCommandPool, float maxAnisotropy);

		void Draw(const vk::CommandBuffer& commandBuffer, const Camera& camera, const vk::Extent2D& viewSize, uint32_t currentFrameIndex);

		virtual void IncrementSize();

	private:
		bool SetupVertexBuffers(VmaAllocator allocator, uint32_t id);
		bool SetupIndexBuffer(VmaAllocator allocator, uint32_t id);
		bool SetupUniformBuffers(VmaAllocator allocator, uint32_t id);
		bool SetupRenderImage(VmaAllocator allocator, const Device& device, uint32_t id, float maxAnisotropy);
		bool CreateMeshResources(VmaAllocator allocator, const Device& device, uint32_t id, float maxAnisotropy);

		bool CreateStagingBuffer(VmaAllocator allocator, const Device& device,
			const CommandPool& resourceCommandPool, const Buffer* destinationBuffer, const void* data,
			uint64_t size, std::vector<std::unique_ptr<Buffer>>& copyBufferCollection,
			std::vector<vk::UniqueCommandBuffer>& copyCommandCollection);

		bool CreateImageStagingBuffer(VmaAllocator allocator, const Device& device,
			const CommandPool& resourceCommandPool, const RenderImage* destinationImage, const void* data, uint64_t size,
			std::vector<std::unique_ptr<Buffer>>& copyBufferCollection, 
			std::vector<vk::UniqueCommandBuffer>& copyCommandCollection);

		const uint32_t m_maxConcurrentFrames;

		std::vector<uint32_t> m_vertexCounts;
		std::vector<uint32_t> m_indexCounts;
		std::unique_ptr<RenderImage> m_blankImage;
		std::unique_ptr<ImageView> m_blankImageView;

		std::vector<std::vector<std::unique_ptr<Buffer>>> m_vertexBuffers;
		std::vector<std::unique_ptr<Buffer>> m_indexBuffers;
		std::vector<std::unique_ptr<RenderImage>> m_renderImages;
		std::vector<std::unique_ptr<ImageView>> m_renderImageViews;
		std::unique_ptr<ImageSampler> m_sampler;

		std::vector<std::vector<std::unique_ptr<Buffer>>> m_uniformBufferArrays;
		std::vector<std::vector<void*>> m_uniformBuffersMappedArrays;

		std::vector<std::unique_ptr<DescriptorPool>> m_descriptorPools;
		std::vector<std::vector<vk::DescriptorSet>> m_descriptorSetArrays;
	};
}