#pragma once

#include "ChunkTypeInfo.hpp"
#include <filesystem>
#include <unordered_map>
#include <vector>
#include <span>

namespace Engine
{
	class AsyncData;

	struct ChunkMemoryEntry
	{
		uint64_t Offset;
		uint64_t Size;

		ChunkMemoryEntry()
			: Offset(0)
			, Size(0)
		{
		}

		ChunkMemoryEntry(uint64_t offset, size_t size)
			: Offset(offset)
			, Size(size)
		{
		}
	};

	struct ImageData
	{
		ImageHeader Header;
		ChunkMemoryEntry Entry;
		std::vector<std::span<const uint8_t>> Spans;

		ImageData(ImageHeader header)
			: Header(header)
		{
		}

		ImageData(ImageHeader header, ChunkMemoryEntry entry)
			: ImageData(header)
		{
			Entry = entry;
		}

		ImageData(ImageHeader header, ChunkMemoryEntry entry, const std::vector<uint8_t>& memory)
			: ImageData(header, entry)
		{
			Spans.resize(header.MipLevels);
			uint64_t offset = entry.Offset;
			uint64_t size = header.FirstMipSize;
			for (uint32_t i = 0; i < header.MipLevels; ++i)
			{
				Spans[i] = std::span<const uint8_t>(memory.begin() + offset, size);
				offset += size;
				size /= 4;
			}

		}
	};

	class ChunkData
	{
	public:
		ChunkData();

		bool WriteToFile(const std::filesystem::path& path, AsyncData* asyncData) const;
		bool Parse(const std::filesystem::path& path, AsyncData* asyncData);

		bool GetVertexData(VertexBufferType type, std::span<uint8_t>& data);
		void SetVertexData(VertexBufferType type, const std::vector<uint8_t>& data);

		bool GetGenericData(uint32_t identifier, std::span<uint8_t>& data);
		void SetGenericData(uint32_t identifier, const std::vector<uint8_t>& data);

		bool GetImageData(std::vector<ImageData>** imageData);
		void AddImageData(const ImageHeader& image, const std::vector<std::vector<uint8_t>>& mipMaps);

		bool LoadedFromDisk() const;

	private:
		bool m_loadedFromDisk;
		std::vector<uint8_t> m_memory;
		std::vector<ImageData> m_imageData;
		std::unordered_map<VertexBufferType, ChunkMemoryEntry> m_vertexDataMap;
		std::unordered_map<uint32_t, ChunkMemoryEntry> m_genericDataMap;
	};
}