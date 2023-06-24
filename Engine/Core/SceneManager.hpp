#pragma once

#include <glm/glm.hpp>
#include "Macros.hpp"
#include "Colour.hpp"
#include "Image.hpp"
#include "VertexData.hpp"
#include "Rendering/MeshInfo.hpp"
#include <vector>
#include <stack>
#include <mutex>
#include <memory>
#include <unordered_map>
#include <string>
#include <atomic>

namespace Engine::Rendering
{
	class Renderer;
	class IGeometryBatch;
}

namespace Engine
{
	class ChunkData;
	class AsyncData;

	class SceneManager
	{
	public:
		SceneManager();

		EXPORT void LoadScene(const std::string& filePath, Engine::Rendering::Renderer* renderer, bool cache, AsyncData& asyncData);

	private:
		void LoadSceneImp(const std::string& filePath, Engine::Rendering::IGeometryBatch* geometryBatch, bool cache, AsyncData& asyncData);
		std::atomic<bool> m_creating;
	};
}