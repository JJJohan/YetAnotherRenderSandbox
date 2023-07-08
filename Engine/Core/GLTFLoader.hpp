#pragma once

#include <filesystem>

namespace Engine::Rendering
{
	class GeometryBatch;
}

namespace Engine
{
	class AsyncData;

	class GLTFLoader
	{
	public:
		bool LoadGLTF(const std::filesystem::path& filePath, Engine::Rendering::GeometryBatch& geometryBatch, AsyncData* asyncData);
	};
}