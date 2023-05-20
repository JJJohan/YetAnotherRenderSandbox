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
		uint64_t UncompressedSize;

		ChunkMemoryEntry()
			: Offset(0)
			, Size(0)
			, UncompressedSize(0)
		{
		}

		ChunkMemoryEntry(uint64_t offset, size_t size, size_t uncompressedSize = 0)
			: Offset(offset)
			, Size(size)
			, UncompressedSize(uncompressedSize)
		{
		}
	};

	struct ImageData
	{
		ImageHeader Header;
		ChunkMemoryEntry Entry;

		ImageData(ImageHeader header)
			: Header(header)
		{
		}

		ImageData(ImageHeader header, ChunkMemoryEntry entry)
			: ImageData(header)
		{
			Entry = entry;
		}
	};

	class ChunkData
	{
	public:
		ChunkData();

		bool WriteToFile(const std::filesystem::path& path, AsyncData* asyncData) const;
		bool Parse(const std::filesystem::path& path, AsyncData* asyncData);

		bool GetVertexData(VertexBufferType type, ChunkMemoryEntry& data);
		void SetVertexData(VertexBufferType type, const std::vector<uint8_t>& data);

		bool GetGenericData(uint32_t identifier, ChunkMemoryEntry& data);
		void SetGenericData(uint32_t identifier, const std::vector<uint8_t>& data);

		bool GetImageData(std::vector<ImageData>** imageData);
		void AddImageData(const ImageHeader& image, const std::vector<std::vector<uint8_t>>& mipMaps);

		bool LoadedFromDisk() const;

		void Decompress(const ChunkMemoryEntry& entry, std::vector<uint8_t>& decompressBuffer) const;

		inline const std::span<const uint8_t>& GetSpan(const ChunkMemoryEntry& data) const
		{
			return std::span<const uint8_t>(m_memory.begin() + data.Offset, data.Size);
		}

	private:
		bool m_loadedFromDisk;
		std::vector<uint8_t> m_memory;
		std::vector<ImageData> m_imageData;
		std::unordered_map<VertexBufferType, ChunkMemoryEntry> m_vertexDataMap;
		std::unordered_map<uint32_t, ChunkMemoryEntry> m_genericDataMap;
	};
}