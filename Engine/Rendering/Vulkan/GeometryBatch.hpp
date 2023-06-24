#pragma once

#include "../Resources/IGeometryBatch.hpp"
#include "../Resources/IndexedIndirectCommand.hpp"
#include <memory>
#include <vector>
#include <stack>
#include <unordered_map>
#include <glm/glm.hpp>

namespace Engine
{
	class ChunkData;
	class AsyncData;
}

namespace Engine::Rendering
{
	class Camera;
	class IDevice;
	class IPhysicalDevice;
	class ICommandBuffer;
	class IBuffer;
	class IRenderImage;
	class IImageSampler;
	class IResourceFactory;
	class IMaterialManager;
	class Material;
	struct MeshInfo;
}

namespace Engine::Rendering::Vulkan
{
	class VulkanRenderer;

	class GeometryBatch : public IGeometryBatch
	{
	public:
		GeometryBatch(VulkanRenderer& renderer);

		bool Initialise(const IPhysicalDevice& physicalDevice, const IDevice& device, 
			const IResourceFactory& resourceFactory, const IMaterialManager& materialManager);

		virtual bool CreateMesh(const std::vector<VertexData>& vertexData,
			const std::vector<uint32_t>& indices,
			const glm::mat4& transform,
			const Colour& colour,
			std::shared_ptr<Image> diffuseImage,
			std::shared_ptr<Image> normalImage,
			std::shared_ptr<Image> metallicRoughnessImage) override;

		virtual bool Optimise() override;
		virtual bool Build(ChunkData* chunkData, AsyncData& asyncData) override;

		virtual void Draw(const ICommandBuffer& commandBuffer, uint32_t currentFrameIndex) override;
		virtual void DrawShadows(const ICommandBuffer& commandBuffer, uint32_t currentFrameIndex, uint32_t cascadeIndex) override;
	private:
		enum class CachedDataType
		{
			IndexBuffer,
			MeshInfo,
			IndirectDrawBuffer
		};

		bool SetupIndirectDrawBuffer(const ICommandBuffer& commandBuffer, ChunkData* chunkData,
			std::vector<std::unique_ptr<IBuffer>>& temporaryBuffers, const IResourceFactory& resourceFactory);

		bool SetupVertexBuffers(const ICommandBuffer& commandBuffer, ChunkData* chunkData,
			std::vector<std::unique_ptr<IBuffer>>& temporaryBuffers, const IResourceFactory& resourceFactory);

		bool SetupIndexBuffer(const ICommandBuffer& commandBuffer, ChunkData* chunkData,
			std::vector<std::unique_ptr<IBuffer>>& temporaryBuffers, const IResourceFactory& resourceFactory);

		bool SetupRenderImage(AsyncData* asyncData, const IDevice& device, const IPhysicalDevice& physicalDevice, const ICommandBuffer& commandBuffer, ChunkData* chunkData,
			std::vector<std::unique_ptr<IBuffer>>& temporaryBuffers, const IResourceFactory& resourceFactory, float maxAnisotropy, uint32_t& imageCount);

		bool SetupMeshInfoBuffer(const ICommandBuffer& commandBuffer, ChunkData* chunkData,
			std::vector<std::unique_ptr<IBuffer>>& temporaryBuffers, const IResourceFactory& resourceFactory);

		bool CreateStagingBuffer(const IResourceFactory& resourceFactory,
			const ICommandBuffer& commandBuffer, const IBuffer* destinationBuffer, const void* data,
			uint64_t size, std::vector<std::unique_ptr<IBuffer>>& copyBufferCollection);

		bool CreateImageStagingBuffer(const IResourceFactory& resourceFactory,
			const ICommandBuffer& commandBuffer, const IRenderImage* destinationImage, uint32_t mipLevel, const void* data, uint64_t size,
			std::vector<std::unique_ptr<IBuffer>>& copyBufferCollection);

		VulkanRenderer& m_renderer;
		std::unique_ptr<IImageSampler> m_sampler;
		std::unique_ptr<IBuffer> m_indirectDrawBuffer;
		std::vector<std::unique_ptr<IBuffer>> m_vertexBuffers;
		std::unique_ptr<IBuffer> m_indexBuffer;
		std::unique_ptr<IBuffer> m_meshInfoBuffer;
		std::vector<std::unique_ptr<IRenderImage>> m_imageArray;

		std::vector<uint32_t> m_vertexOffsets;
		std::vector<uint32_t> m_indexOffsets;
		std::vector<uint32_t> m_indexCounts;

		std::stack<uint32_t> m_recycledIds;
		std::vector<bool> m_active;
		std::atomic<bool> m_creating;
		uint32_t m_meshCapacity;

		std::vector<std::vector<std::unique_ptr<VertexData>>> m_vertexDataArrays;
		std::vector<std::unique_ptr<std::vector<uint32_t>>> m_indexArrays;
		std::vector<Engine::Rendering::MeshInfo> m_meshInfos;
		std::vector<std::shared_ptr<Image>> m_images;
		std::vector<uint32_t> m_vertexBufferOffsets;
		std::vector<uint32_t> m_indexBufferOffsets;

		std::unordered_map<uint64_t, size_t> m_imageHashTable;
		std::unordered_map<uint64_t, size_t> m_vertexDataHashTable;
		std::unordered_map<uint64_t, size_t> m_indexDataHashTable;

		std::vector<IndexedIndirectCommand> m_indirectDrawCommands;
		Material* m_pbrMaterial;
		Material* m_shadowMaterial;
	};
}