#pragma once

#include <filesystem>
#include "VertexData.hpp"

namespace Engine::Rendering
{
	class SceneManager;

	class GLTFLoader
	{
	public:
		bool LoadGLTF(const std::filesystem::path& filePath, SceneManager* sceneManager);
	};
}