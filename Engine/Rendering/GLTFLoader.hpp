#pragma once

#include <string>
#include "VertexData.hpp"

namespace Engine::Rendering
{
	class SceneManager;
	class Shader;

	class GLTFLoader
	{
	public:
		EXPORT bool LoadGLTF(const std::string& filePath, SceneManager* sceneManager, std::vector<uint32_t>& results);
	};
}