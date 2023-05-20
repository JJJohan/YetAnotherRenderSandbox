#include "MeshOptimiser.hpp"
#include <meshoptimizer.h>
#include "Logging/Logger.hpp"
#include "Rendering/VertexData.hpp"
#include <glm/glm.hpp>

using namespace Engine::Logging;
using namespace Engine::Rendering;

namespace Engine
{
	bool MeshOptimiser::Optimise(std::vector<uint32_t>& indices, std::vector<std::unique_ptr<VertexData>>& vertexArrays)
	{
		auto optimiseStartTime = std::chrono::high_resolution_clock::now();

		if (vertexArrays.empty())
			return false;

		size_t indexCount = indices.size();
		size_t vertexCount = vertexArrays[0]->GetCount();
		size_t originalVertexCount = vertexCount;

		if (indexCount == 0 || vertexCount == 0)
		{
			return false;
		}

		std::vector<meshopt_Stream> streams;
		for (auto it = vertexArrays.begin(); it != vertexArrays.end(); ++it)
		{
			const VertexData* vertexData = it->get();
			streams.emplace_back(meshopt_Stream{ vertexData->GetData(), vertexData->GetElementSize(), vertexData->GetElementSize() });
		}

		std::vector<uint32_t> remap(indexCount);
		vertexCount = meshopt_generateVertexRemapMulti(remap.data(), indices.data(), indexCount, vertexCount, streams.data(), streams.size());
		if (vertexCount == 0)
		{
			return false;
		}

		std::vector<uint32_t> optimisedIndices(indexCount);
		meshopt_remapIndexBuffer(optimisedIndices.data(), indices.data(), indexCount, remap.data());

		std::vector<uint32_t> vertexRemap(vertexCount);
		vertexCount = meshopt_optimizeVertexFetchRemap(vertexRemap.data(), indices.data(), indexCount, vertexCount);

		for (auto it = vertexArrays.begin(); it != vertexArrays.end(); ++it)
		{
			VertexData* vertexData = it->get();
			std::vector<uint8_t> optimisedVertexData(static_cast<size_t>(vertexData->GetCount()) * vertexData->GetElementSize());
			meshopt_remapVertexBuffer(optimisedVertexData.data(), vertexData->GetData(), originalVertexCount, vertexData->GetElementSize(), vertexRemap.data());
			vertexData->ReplaceData(optimisedVertexData, static_cast<uint32_t>(vertexCount));
		}

		meshopt_optimizeVertexCache(optimisedIndices.data(), optimisedIndices.data(), indexCount, vertexCount);

		meshopt_optimizeOverdraw(optimisedIndices.data(), optimisedIndices.data(), indexCount, static_cast<const float*>(vertexArrays[0]->GetData()), vertexCount, sizeof(glm::vec3), 1.05f);

		indices.clear();
		indices.append_range(optimisedIndices);

		auto optimiseEndTime = std::chrono::high_resolution_clock::now();
		float optimiseDeltaTime = std::chrono::duration<float, std::chrono::seconds::period>(optimiseEndTime - optimiseStartTime).count();
		Logger::Verbose("Mesh optimising finished in {} seconds.", optimiseDeltaTime);

		return true;
	}
}