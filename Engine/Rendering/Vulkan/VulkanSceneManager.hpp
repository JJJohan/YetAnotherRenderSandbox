#pragma once

#include "../SceneManager.hpp"
#include "../Resources/IndexedIndirectCommand.hpp"
#include <memory>

namespace Engine
{
	class Image;
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
	class IImageView;
	class IRenderImage;
	class IImageSampler;
	class IResourceFactory;
	class IMaterialManager;
	class Material;
}

namespace Engine::Rendering::Vulkan
{
	class VulkanRenderer;

	class VulkanSceneManager : public SceneManager
	{
	public:
		VulkanSceneManager(VulkanRenderer& renderer);

		bool Initialise(const IPhysicalDevice& physicalDevice, const IDevice& device, 
			const IResourceFactory& resourceFactory, const IMaterialManager& materialManager);

		virtual bool Build(ChunkData* chunkData, AsyncData& asyncData) override;

		void Draw(const ICommandBuffer& commandBuffer, uint32_t currentFrameIndex);
		void DrawShadows(const ICommandBuffer& commandBuffer, uint32_t currentFrameIndex, uint32_t cascadeIndex);

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
		std::vector<std::unique_ptr<IImageView>> m_imageArrayView;

		std::vector<uint32_t> m_vertexOffsets;
		std::vector<uint32_t> m_indexOffsets;
		std::vector<uint32_t> m_indexCounts;

		std::vector<IndexedIndirectCommand> m_indirectDrawCommands;
		Material* m_pbrMaterial;
		Material* m_shadowMaterial;
	};
}