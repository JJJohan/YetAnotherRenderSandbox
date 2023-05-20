#pragma once

#include <filesystem>
#include "VertexData.hpp"

namespace Engine
{
	class AsyncData;
}

namespace Engine::Rendering
{
	class SceneManager;

	class GLTFLoader
	{
	public:
		bool LoadGLTF(const std::filesystem::path& filePath, SceneManager* sceneManager, AsyncData* asyncData);
	};
}