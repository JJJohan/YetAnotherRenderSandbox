#pragma once

#include <filesystem>

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