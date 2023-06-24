#pragma once

#include <filesystem>

namespace Engine::Rendering
{
	class IGeometryBatch;
}

namespace Engine
{
	class AsyncData;

	class GLTFLoader
	{
	public:
		bool LoadGLTF(const std::filesystem::path& filePath, Engine::Rendering::IGeometryBatch* geometryBatch, AsyncData* asyncData);
	};
}