#pragma once

#include <memory>
#include <vector>
#include <stack>
#include <unordered_map>
#include <glm/glm.hpp>
#include "../Resources/IndexedIndirectCommand.hpp"
#include "Core/VertexData.hpp"
#include "../Resources/MeshInfo.hpp"

namespace Engine
{
	class ChunkData;
	class AsyncData;
	class Image;
	struct Colour;
}

namespace Engine::Rendering
{
	class Camera;
	class IDevice;
	class IPhysicalDevice;
	class ICommandBuffer;
	class IResourceFactory;
	class Renderer;
	class IBuffer;
	class IRenderImage;

	class GeometryBatch
	{
	public:
		GeometryBatch(Renderer& renderer);

		bool CreateMesh(const std::vector<VertexData>& vertexData,
			const std::vector<uint32_t>& indices,
			const glm::mat4& transform,
			const Colour& colour,
			std::shared_ptr<Image> diffuseImage,
			std::shared_ptr<Image> normalImage,
			std::shared_ptr<Image> metallicRoughnessImage,
			bool convertToLHS);

		bool Optimise();
		bool Build(ChunkData* chunkData, AsyncData& asyncData);

		inline const IBuffer& GetIndirectDrawBuffer() const { return *m_indirectDrawBuffer; }
		inline const IBuffer& GetBoundsBuffer() const { return *m_boundsBuffer; }
		inline const std::vector<std::unique_ptr<IBuffer>>& GetVertexBuffers() const { return m_vertexBuffers; }
		inline const IBuffer& GetIndexBuffer() const { return *m_indexBuffer; }
		inline const IBuffer& GetMeshInfoBuffer() const { return *m_meshInfoBuffer; }
		inline const std::vector<std::unique_ptr<IRenderImage>>& GetImages() const { return m_imageArray; }
		inline bool IsBuilt() const { return !m_creating; }
		inline uint32_t GetMeshCapacity() const { return m_meshCapacity; }

	private:
		enum class CachedDataType
		{
			IndexBuffer,
			MeshInfo,
			IndirectDrawBuffer,
			BoundsBuffer
		};

		bool SetupIndirectDrawBuffer(const ICommandBuffer& commandBuffer, ChunkData* chunkData,
			std::vector<std::unique_ptr<IBuffer>>& temporaryBuffers, const IResourceFactory& resourceFactory);

		bool SetupBoundsBuffer(const ICommandBuffer& commandBuffer, ChunkData* chunkData,
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

		Renderer& m_renderer;

		std::unique_ptr<IBuffer> m_indirectDrawBuffer;
		std::vector<std::unique_ptr<IBuffer>> m_vertexBuffers;
		std::unique_ptr<IBuffer> m_indexBuffer;
		std::unique_ptr<IBuffer> m_boundsBuffer;
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
	};
}