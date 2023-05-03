#pragma once

#include <string>
#include "VertexData.hpp"

namespace Engine::Rendering
{
	class MeshManager;
	class Shader;

	class GLTFLoader
	{
	public:
		EXPORT bool LoadGLTF(const std::string& filePath, MeshManager* meshManager, const Shader* shader, std::vector<uint32_t>& results);
	};
}