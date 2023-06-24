#pragma once

#include "../Types.hpp"
#include <glm/glm.hpp>
#include "Core/VertexData.hpp"
#include <memory>

namespace Engine
{
	class ChunkData;
	class AsyncData;
	class Image;
	struct Colour;
}

namespace Engine::Rendering
{
	class ICommandBuffer;

	class IGeometryBatch
	{
	public:
		IGeometryBatch() = default;
		virtual ~IGeometryBatch() = default;

		virtual bool CreateMesh(const std::vector<VertexData>& vertexData,
			const std::vector<uint32_t>& indices,
			const glm::mat4& transform,
			const Colour& colour,
			std::shared_ptr<Image> diffuseImage,
			std::shared_ptr<Image> normalImage,
			std::shared_ptr<Image> metallicRoughnessImage) = 0;

		virtual bool Optimise() = 0;
		virtual bool Build(ChunkData* chunkData, AsyncData& asyncState) = 0;

		// Temporary - to be moved to render passes.
		virtual void Draw(const ICommandBuffer& commandBuffer, uint32_t currentFrameIndex) = 0;
		virtual void DrawShadows(const ICommandBuffer& commandBuffer, uint32_t currentFrameIndex, uint32_t cascadeIndex) = 0;
	};
}