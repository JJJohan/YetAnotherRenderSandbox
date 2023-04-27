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

namespace Engine::Rendering::Vulkan
{
	class Buffer;
	class Device;
	class PhysicalDevice;
	class PipelineLayout;
	class CommandPool;
	class CommandBuffer;
	class DescriptorPool;

	class VulkanMeshManager : public MeshManager
	{
	public:
		VulkanMeshManager(const uint32_t maxConcurrentFrames);

		virtual uint32_t CreateMesh(
			const Shader* shader,
			const std::vector<glm::vec3>& positions,
			const std::vector<Colour>& vertexColours,
			const std::vector<uint32_t>& indices,
			const Colour& colour,
			const glm::mat4& transform,
			std::shared_ptr<Image> image);

		virtual void DestroyMesh(uint32_t id);

		bool Update(VmaAllocator allocator, const Device& device, const CommandPool& resourceCommandPool);

		void Draw(const vk::CommandBuffer& commandBuffer, const vk::Extent2D& viewSize, uint32_t currentFrameIndex);

		virtual void IncrementSize();

	private:
		bool SetupPositionBuffer(VmaAllocator allocator, uint32_t id);
		bool SetupColourBuffer(VmaAllocator allocator, uint32_t id);
		bool SetupIndexBuffer(VmaAllocator allocator, uint32_t id);
		bool SetupUniformBuffers(VmaAllocator allocator, uint32_t id);
		bool CreateMeshResources(VmaAllocator allocator, const Device& device, uint32_t id);

		bool CreateStagingBuffer(VmaAllocator allocator, const Device& device,
			const CommandPool& resourceCommandPool, const Buffer* destinationBuffer, const void* data,
			uint64_t size, std::vector<std::unique_ptr<Buffer>>& copyBufferCollection,
			std::vector<vk::UniqueCommandBuffer>& copyCommandCollection);

		const uint32_t m_maxConcurrentFrames;

		std::vector<uint32_t> m_vertexCounts;
		std::vector<uint32_t> m_indexCounts;

		std::vector<std::unique_ptr<Buffer>> m_positionBuffers;
		std::vector<std::unique_ptr<Buffer>> m_colourBuffers;
		std::vector<std::unique_ptr<Buffer>> m_indexBuffers;

		std::vector<std::vector<std::unique_ptr<Buffer>>> m_uniformBufferArrays;
		std::vector<std::vector<void*>> m_uniformBuffersMappedArrays;

		std::vector<std::unique_ptr<DescriptorPool>> m_descriptorPools;
		std::vector<std::vector<vk::DescriptorSet>> m_descriptorSetArrays;

		std::vector<const PipelineLayout*> m_pipelineLayouts;
	};
}