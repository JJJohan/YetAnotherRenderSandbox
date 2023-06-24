#include "SceneManager.hpp"
#include "Logging/Logger.hpp"
#include "ChunkData.hpp"
#include "Utilities.hpp"
#include "AsyncData.hpp"
#include <filesystem>
#include "GltfLoader.hpp"
#include "Rendering/Renderer.hpp"
#include "Rendering/Resources/IGeometryBatch.hpp"

using namespace Engine::Logging;
using namespace Engine::Rendering;

namespace Engine
{
	SceneManager::SceneManager()
		: m_creating(false)
	{
	}

	void SceneManager::LoadSceneImp(const std::string& filePath, IGeometryBatch* geometryBatch, bool cache, AsyncData& asyncData)
	{
		std::filesystem::path path(filePath);
		std::filesystem::path chunkPath(path);
		chunkPath.replace_extension("chunk");
		if (cache)
		{
			if (std::filesystem::exists(chunkPath))
			{
				std::filesystem::file_time_type chunkWriteTime(std::filesystem::last_write_time(chunkPath));
				std::filesystem::file_time_type fileWriteTime(std::filesystem::last_write_time(path));
				if (fileWriteTime > chunkWriteTime)
				{
					Logger::Info("Scene cache file out of date, rebuilding.");
				}
				else
				{
					asyncData.InitSubProgress("Loading Cache", 1000.0f);
					ChunkData chunkData{};
					if (chunkData.Parse(chunkPath, &asyncData))
					{
						asyncData.InitSubProgress("Uploading Cache Data", 500.0f);
						if (geometryBatch->Build(&chunkData, asyncData))
						{
							m_creating = false;
							asyncData.State = AsyncState::Completed;
							return;
						}
					}

					asyncData.InitProgress("Loading Scene", 1500.0f);
				}
			}
		}

		if (!std::filesystem::exists(path))
		{
			Logger::Error("Scene file does not exist.");
			asyncData.State = AsyncState::Failed;
			m_creating = false;
			return;
		}

		Image::CompressInit();

		std::string pathExtension(path.extension().string());
		if (Utilities::EqualsIgnoreCase(pathExtension, ".glb") || Utilities::EqualsIgnoreCase(pathExtension, ".gltf"))
		{
			asyncData.InitSubProgress("Loading GLTF Data", 400.0f);
			GLTFLoader gltfLoader;
			if (!gltfLoader.LoadGLTF(path, geometryBatch, &asyncData))
			{
				asyncData.State = AsyncState::Failed;
				m_creating = false;
				return;
			}
		}
		else
		{
			Logger::Error("Scene file type not handled.");
			asyncData.State = AsyncState::Failed;
			m_creating = false;
			return;
		}

		if (asyncData.State == AsyncState::Cancelled)
		{
			m_creating = false;
			return;
		}

		asyncData.InitSubProgress("Optimising Mesh", 100.0f);
		if (!geometryBatch->Optimise())
		{
			Logger::Error("Error occurred while optimising mesh.");
			asyncData.State = AsyncState::Failed;
			m_creating = false;
			return;
		}

		if (asyncData.State == AsyncState::Cancelled)
		{
			m_creating = false;
			return;
		}

		asyncData.InitSubProgress("Building Graphics Resources", 100.0f);
		ChunkData chunkData{};
		bool buildSuccess = geometryBatch->Build(cache ? &chunkData : nullptr, asyncData);
		if (buildSuccess && cache && asyncData.State == AsyncState::InProgress)
		{
			asyncData.InitSubProgress("Writing Cache", 500.0f);
			if (!chunkData.WriteToFile(chunkPath, &asyncData))
			{
				Logger::Error("Failed to write imported scene data to file, data was not cached.");
			}
		}

		m_creating = false;
		asyncData.State = AsyncState::Completed;
	}

	void SceneManager::LoadScene(const std::string& filePath, Renderer* renderer, bool cache, AsyncData& asyncData)
	{
		if (m_creating)
		{
			Logger::Error("Cannot load more than one scene simultaneously.");
			asyncData.State = AsyncState::Failed;
			return;
		}

		IGeometryBatch* geometryBatch = nullptr;
		if (!renderer->PrepareSceneGeometryBatch(&geometryBatch))
		{
			Logger::Error("Failed to prepare scene geometry batch.");
			asyncData.State = AsyncState::Failed;
			return;
		}

		m_creating = true;
		asyncData.State = AsyncState::InProgress;
		asyncData.InitProgress("Loading Scene", cache ? 1500.0f : 1000.0f);
		asyncData.SetFuture(std::move(std::async(std::launch::async, [filePath, geometryBatch, cache, &asyncData, this] { LoadSceneImp(filePath, geometryBatch, cache, asyncData); })));
	}
}