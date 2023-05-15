#include "ChunkData.hpp"
#include "Logging/Logger.hpp"
#include <fstream>
#include <chrono>

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

	bool ChunkData::WriteToFile(const std::filesystem::path& path) const
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

		ChunkHeader header{};
		header.ResourceCount = static_cast<uint32_t>(resourceCount);
		stream.write(reinterpret_cast<const char*>(&header), sizeof(ChunkHeader));

		for (const auto& data : m_genericDataMap)
		{			
			ChunkResourceHeader resourceHeader;
			resourceHeader.Identifier = data.first;
			resourceHeader.ResourceType = ChunkResourceType::Generic;
			resourceHeader.ResourceSize = data.second.Size;
			stream.write(reinterpret_cast<const char*>(&resourceHeader), sizeof(ChunkResourceHeader));
			std::copy(m_memory.begin() + data.second.Offset, m_memory.begin() + data.second.Offset + data.second.Size, std::ostreambuf_iterator<char>(stream));
		}

		for (const auto& data : m_vertexDataMap)
		{
			ChunkResourceHeader resourceHeader;
			resourceHeader.ResourceType = ChunkResourceType::VertexBuffer;
			resourceHeader.ResourceSize = data.second.Size;
			stream.write(reinterpret_cast<const char*>(&resourceHeader), sizeof(ChunkResourceHeader));
			VertexBufferHeader vertexHeader;
			vertexHeader.Type = data.first;
			stream.write(reinterpret_cast<const char*>(&vertexHeader), sizeof(VertexBufferHeader));
			std::copy(m_memory.begin() + data.second.Offset, m_memory.begin() + data.second.Offset + data.second.Size, std::ostreambuf_iterator<char>(stream));
		}

		for (const auto& data : m_imageData)
		{
			ChunkResourceHeader resourceHeader;
			resourceHeader.ResourceType = ChunkResourceType::Image;
			resourceHeader.ResourceSize = data.Entry.Size;
			stream.write(reinterpret_cast<const char*>(&resourceHeader), sizeof(ChunkResourceHeader));
			stream.write(reinterpret_cast<const char*>(&data.Header), sizeof(ImageHeader));
			std::copy(m_memory.begin() + data.Entry.Offset, m_memory.begin() + data.Entry.Offset + data.Entry.Size, std::ostreambuf_iterator<char>(stream));
		}

		bool success = stream.good();
		stream.flush();
		stream.close();

		auto writeEndTime = std::chrono::high_resolution_clock::now();
		float saveDeltaTime = std::chrono::duration<float, std::chrono::seconds::period>(writeEndTime - writeStartTime).count();
		Logger::Verbose("Chunk saved to disk in {} seconds.", saveDeltaTime);

		return success;
	}

	bool ChunkData::Parse(const std::filesystem::path& path)
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

		// Currently, just read the entire file into memory.
		m_memory.resize(size);
		stream.read(reinterpret_cast<char*>(m_memory.data()), size);
		stream.close();
		uint64_t dataIndex = 0;

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

		for (uint32_t i = 0; i < header.ResourceCount; ++i)
		{
			ChunkResourceHeader resource;
			memcpy(&resource, m_memory.data() + dataIndex, sizeof(ChunkResourceHeader));
			dataIndex += sizeof(ChunkResourceHeader);

			switch (resource.ResourceType)
			{
			case ChunkResourceType::Generic:
				m_genericDataMap[resource.Identifier] = ChunkMemoryEntry(dataIndex, resource.ResourceSize);
				dataIndex += resource.ResourceSize;
				break;

			case ChunkResourceType::VertexBuffer:
				VertexBufferHeader vertexData;
				memcpy(&vertexData, m_memory.data() + dataIndex, sizeof(VertexBufferHeader));
				dataIndex += sizeof(VertexBufferHeader);

				m_vertexDataMap[vertexData.Type] = ChunkMemoryEntry(dataIndex, resource.ResourceSize);
				dataIndex += resource.ResourceSize;
				break;

			case ChunkResourceType::Image:
			{
				ImageHeader imageHeader;
				memcpy(&imageHeader, m_memory.data() + dataIndex, sizeof(ImageHeader));
				dataIndex += sizeof(ImageHeader);

				m_imageData.emplace_back(ImageData(imageHeader.Width, imageHeader.Height, imageHeader.Srgb, ChunkMemoryEntry(dataIndex, resource.ResourceSize), m_memory));
				dataIndex += resource.ResourceSize;
			}
			break;

			default:
				Logger::Error("Input file '{}' contains malformed data.", path.string());
				return false;
			}
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

	bool ChunkData::GetVertexData(VertexBufferType type, std::span<uint8_t>& data)
	{
		auto result = m_vertexDataMap.find(type);
		if (result != m_vertexDataMap.end())
		{
			data = std::span<uint8_t>(m_memory.begin() + result->second.Offset, result->second.Size);
			return true;
		}

		return false;
	}

	void ChunkData::SetVertexData(VertexBufferType type, const std::vector<uint8_t>& data)
	{
		if (m_vertexDataMap.contains(type))
		{
			Logger::Warning("Replacing existing vertex type '{}' in ChunkData.", static_cast<uint32_t>(type));
		}

		uint64_t offset = m_memory.size();
		m_memory.insert(m_memory.end(), data.begin(), data.end());
		m_vertexDataMap[type] = ChunkMemoryEntry(offset, data.size());
	}

	bool ChunkData::GetGenericData(uint32_t identifier, std::span<uint8_t>& data)
	{
		auto result = m_genericDataMap.find(identifier);
		if (result != m_genericDataMap.end())
		{
			data = std::span<uint8_t>(m_memory.begin() + result->second.Offset, result->second.Size);
			return true;
		}

		return false;
	}

	void ChunkData::SetGenericData(uint32_t identifier, const std::vector<uint8_t>& data)
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

	void ChunkData::AddImageData(const ImageHeader& image, const std::vector<uint8_t>& pixelData)
	{
		uint64_t offset = m_memory.size();
		m_memory.insert(m_memory.end(), pixelData.begin(), pixelData.end());
		m_imageData.emplace_back(ImageData(image.Width, image.Height, image.Srgb, ChunkMemoryEntry(offset, pixelData.size())));
	}
}