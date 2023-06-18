#include "ChunkData.hpp"
#include "AsyncData.hpp"
#include "Logging/Logger.hpp"
#include <fstream>
#include <chrono>
#include <lz4.h>

using namespace Engine::Logging;

namespace Engine
{
	ChunkData::ChunkData()
		: m_genericDataMap()
		, m_imageData()
		, m_vertexDataMap()
		, m_loadedFromDisk(false)
		, m_memory()
	{
	}

	inline void CompressData(const char* source, size_t size, std::vector<char>& outputBuffer, int32_t& compressedSize)
	{
		const int32_t maxSize = LZ4_compressBound(static_cast<int32_t>(size));
		if (outputBuffer.size() < maxSize)
			outputBuffer.resize(maxSize);

		compressedSize = LZ4_compress_default(source, outputBuffer.data(), static_cast<int32_t>(size), maxSize);
	}

	bool ChunkData::WriteToFile(const std::filesystem::path& path, AsyncData* asyncData) const
	{
		auto writeStartTime = std::chrono::high_resolution_clock::now();

		size_t resourceCount = m_genericDataMap.size() + m_imageData.size() + m_vertexDataMap.size();
		if (resourceCount == 0)
		{
			Logger::Error("Chunk data is empty.");
			return false;
		}

		std::ofstream stream(path, std::ios::binary);
		if (!stream.is_open())
		{
			Logger::Error("Could not open output path '{}'.", path.string());
			return false;
		}

		std::vector<char> compressionBuffer;
		int32_t compressedSize = 0;

		ChunkHeader header{};
		header.ResourceCount = static_cast<uint32_t>(resourceCount);
		stream.write(reinterpret_cast<const char*>(&header), sizeof(ChunkHeader));
		const char* memoryData = reinterpret_cast<const char*>(m_memory.data());

		float subTicks = 500.0f / static_cast<float>(m_genericDataMap.size()) / 3.0f;
		for (const auto& data : m_genericDataMap)
		{
			CompressData(memoryData + data.second.Offset, data.second.Size, compressionBuffer, compressedSize);

			ChunkResourceHeader resourceHeader;
			resourceHeader.Identifier = data.first;
			resourceHeader.ResourceType = ChunkResourceType::Generic;
			resourceHeader.ResourceSize = compressedSize;
			resourceHeader.UncompressedSize = data.second.Size;
			stream.write(reinterpret_cast<const char*>(&resourceHeader), sizeof(ChunkResourceHeader));
			stream.write(compressionBuffer.data(), compressedSize);

			if (asyncData != nullptr)
				asyncData->AddSubProgress(subTicks);
		}

		subTicks = 500.0f / static_cast<float>(m_vertexDataMap.size()) / 3.0f;
		for (const auto& data : m_vertexDataMap)
		{
			CompressData(memoryData + data.second.Offset, data.second.Size, compressionBuffer, compressedSize);

			ChunkResourceHeader resourceHeader;
			resourceHeader.ResourceType = ChunkResourceType::VertexBuffer;
			resourceHeader.ResourceSize = compressedSize;
			resourceHeader.UncompressedSize = data.second.Size;
			stream.write(reinterpret_cast<const char*>(&resourceHeader), sizeof(ChunkResourceHeader));
			VertexBufferHeader vertexHeader;
			vertexHeader.Type = data.first;
			stream.write(reinterpret_cast<const char*>(&vertexHeader), sizeof(VertexBufferHeader));
			stream.write(compressionBuffer.data(), compressedSize);

			if (asyncData != nullptr)
				asyncData->AddSubProgress(subTicks);
		}

		subTicks = 500.0f / static_cast<float>(m_imageData.size()) / 3.0f;
		for (const auto& data : m_imageData)
		{
			CompressData(memoryData + data.Entry.Offset, data.Entry.Size, compressionBuffer, compressedSize);

			ChunkResourceHeader resourceHeader;
			resourceHeader.ResourceType = ChunkResourceType::Image;
			resourceHeader.ResourceSize = compressedSize;
			resourceHeader.UncompressedSize = data.Entry.Size;
			stream.write(reinterpret_cast<const char*>(&resourceHeader), sizeof(ChunkResourceHeader));
			stream.write(reinterpret_cast<const char*>(&data.Header), sizeof(ImageHeader));
			stream.write(compressionBuffer.data(), compressedSize);

			if (asyncData != nullptr)
				asyncData->AddSubProgress(subTicks);
		}

		bool success = stream.good();
		stream.flush();
		stream.close();

		auto writeEndTime = std::chrono::high_resolution_clock::now();
		float saveDeltaTime = std::chrono::duration<float, std::chrono::seconds::period>(writeEndTime - writeStartTime).count();
		Logger::Verbose("Chunk saved to disk in {} seconds.", saveDeltaTime);

		return success;
	}

	void ChunkData::Decompress(const ChunkMemoryEntry& entry, std::vector<uint8_t>& decompressBuffer) const
	{
		// Consider doing decompression on the GPU instead - worth it?
		if (decompressBuffer.size() < entry.UncompressedSize)
			decompressBuffer.resize(entry.UncompressedSize);

		LZ4_decompress_safe(reinterpret_cast<const char*>(m_memory.data() + entry.Offset), 
			reinterpret_cast<char*>(decompressBuffer.data()), static_cast<int32_t>(entry.Size),
			static_cast<int32_t>(entry.UncompressedSize));
	}

	bool ChunkData::Parse(const std::filesystem::path& path, AsyncData* asyncData)
	{
		auto parseStartTime = std::chrono::high_resolution_clock::now();

		std::ifstream stream(path, std::ios::binary);
		if (!stream.is_open())
		{
			Logger::Error("Could not open input path '{}'.", path.string());
			return false;
		}

		stream.seekg(0, std::ios_base::end);
		std::size_t size = stream.tellg();
		stream.seekg(0, std::ios_base::beg);
		if (size < sizeof(ChunkHeader))
		{
			Logger::Error("Input file '{}' was too small to contain chunk data.", path.string());
			return false;
		}

		if (asyncData != nullptr)
			asyncData->AddSubProgress(100.0f);

		// Currently, just read the entire file into memory.
		m_memory.resize(size);
		stream.read(reinterpret_cast<char*>(m_memory.data()), size);
		stream.close();
		uint64_t dataIndex = 0;

		if (asyncData != nullptr)
			asyncData->AddSubProgress(500.0f);

		ChunkHeader header;
		memcpy(&header, m_memory.data(), sizeof(ChunkHeader));
		dataIndex += sizeof(ChunkHeader);

		if (header.Magic != HeaderMagic)
		{
			Logger::Error("Input file '{}' does not contain valid chunk header.", path.string());
			return false;
		}

		if (header.Version != CurrentVersion)
		{
			Logger::Error("Input file '{}' contains newer data which is not compatible.", path.string());
			return false;
		}

		float resourceSubTicks = static_cast<float>(header.ResourceCount) / 400.0f;
		for (uint32_t i = 0; i < header.ResourceCount; ++i)
		{
			ChunkResourceHeader resource;
			memcpy(&resource, m_memory.data() + dataIndex, sizeof(ChunkResourceHeader));
			dataIndex += sizeof(ChunkResourceHeader);

			switch (resource.ResourceType)
			{
			case ChunkResourceType::Generic:
				m_genericDataMap[resource.Identifier] = ChunkMemoryEntry(dataIndex, resource.ResourceSize, resource.UncompressedSize);
				dataIndex += resource.ResourceSize;
				break;

			case ChunkResourceType::VertexBuffer:
				VertexBufferHeader vertexData;
				memcpy(&vertexData, m_memory.data() + dataIndex, sizeof(VertexBufferHeader));
				dataIndex += sizeof(VertexBufferHeader);

				m_vertexDataMap[vertexData.Type] = ChunkMemoryEntry(dataIndex, resource.ResourceSize, resource.UncompressedSize);
				dataIndex += resource.ResourceSize;
				break;

			case ChunkResourceType::Image:
			{
				ImageHeader imageHeader;
				memcpy(&imageHeader, m_memory.data() + dataIndex, sizeof(ImageHeader));
				dataIndex += sizeof(ImageHeader);

				m_imageData.emplace_back(ImageData(imageHeader, ChunkMemoryEntry(dataIndex, resource.ResourceSize, resource.UncompressedSize)));
				dataIndex += resource.ResourceSize;
			}
			break;

			default:
				Logger::Error("Input file '{}' contains malformed data.", path.string());
				return false;
			}

			if (asyncData != nullptr)
				asyncData->AddSubProgress(resourceSubTicks);
		}

		m_loadedFromDisk = true;

		auto parseEndTime = std::chrono::high_resolution_clock::now();
		float loadDeltaTime = std::chrono::duration<float, std::chrono::seconds::period>(parseEndTime - parseStartTime).count();
		Logger::Verbose("Chunk loaded from disk in {} seconds.", loadDeltaTime);

		return true;
	}

	bool ChunkData::LoadedFromDisk() const
	{
		return m_loadedFromDisk;
	}

	bool ChunkData::GetVertexData(VertexBufferType type, ChunkMemoryEntry& data)
	{
		auto result = m_vertexDataMap.find(type);
		if (result != m_vertexDataMap.end())
		{
			data = result->second;
			return true;
		}

		return false;
	}

	void ChunkData::SetVertexData(VertexBufferType type, const std::span<uint8_t>& data)
	{
		if (m_vertexDataMap.contains(type))
		{
			Logger::Warning("Replacing existing vertex type '{}' in ChunkData.", static_cast<uint32_t>(type));
		}

		uint64_t offset = m_memory.size();
		m_memory.insert(m_memory.end(), data.begin(), data.end());
		m_vertexDataMap[type] = ChunkMemoryEntry(offset, data.size());
	}

	bool ChunkData::GetGenericData(uint32_t identifier, ChunkMemoryEntry& data)
	{
		auto result = m_genericDataMap.find(identifier);
		if (result != m_genericDataMap.end())
		{
			data = result->second;
			return true;
		}

		return false;
	}

	void ChunkData::SetGenericData(uint32_t identifier, const std::span<uint8_t>& data)
	{
		if (m_genericDataMap.contains(identifier))
		{
			Logger::Warning("Replacing existing identifier '{}' in ChunkData.", identifier);
		}

		uint64_t offset = m_memory.size();
		m_memory.insert(m_memory.end(), data.begin(), data.end());
		m_genericDataMap[identifier] = ChunkMemoryEntry(offset, data.size());
	}

	bool ChunkData::GetImageData(std::vector<ImageData>** imageData)
	{
		if (!m_imageData.empty())
		{
			*imageData = &m_imageData;
			return true;
		}

		return false;
	}

	void ChunkData::AddImageData(const ImageHeader& image, const std::vector<std::vector<uint8_t>>& mipMaps)
	{
		ImageHeader header = image;
		header.MipLevels = static_cast<uint32_t>(mipMaps.size());
		header.FirstMipSize = mipMaps.front().size();

		uint64_t offset = m_memory.size();
		size_t totalSize = 0;
		for (const auto& data : mipMaps)
		{
			m_memory.insert(m_memory.end(), data.begin(), data.end());
			totalSize += data.size();
		}

		m_imageData.emplace_back(ImageData(header, ChunkMemoryEntry(offset, totalSize)));
	}
}