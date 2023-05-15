#pragma once

#include "ChunkTypeInfo.hpp"
#include <filesystem>
#include <unordered_map>
#include <vector>
#include <span>

namespace Engine
{
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
		std::span<const uint8_t> Span;

		ImageData(uint32_t width, uint32_t height, bool srgb)
		{
			Header.Width = width;
			Header.Height = height;
			Header.Srgb = srgb;
		}

		ImageData(uint32_t width, uint32_t height, bool srgb, ChunkMemoryEntry entry)
			: ImageData(width, height, srgb)
		{
			Entry = entry;
		}

		ImageData(uint32_t width, uint32_t height, bool srgb, ChunkMemoryEntry entry, const std::vector<uint8_t>& memory)
			: ImageData(width, height, srgb)
		{
			Entry = entry;
			Span = std::span<const uint8_t>(memory.begin() + entry.Offset, entry.Size);
		}
	};

	class ChunkData
	{
	public:
		ChunkData();

		bool WriteToFile(const std::filesystem::path& path) const;
		bool Parse(const std::filesystem::path& path);

		bool GetVertexData(VertexBufferType type, std::span<uint8_t>& data);
		void SetVertexData(VertexBufferType type, const std::vector<uint8_t>& data);

		bool GetGenericData(uint32_t identifier, std::span<uint8_t>& data);
		void SetGenericData(uint32_t identifier, const std::vector<uint8_t>& data);

		bool GetImageData(std::vector<ImageData>** imageData);
		void AddImageData(const ImageHeader& image, const std::vector<uint8_t>& pixelData);

		bool LoadedFromDisk() const;

	private:
		bool m_loadedFromDisk;
		std::vector<uint8_t> m_memory;
		std::vector<ImageData> m_imageData;
		std::unordered_map<VertexBufferType, ChunkMemoryEntry> m_vertexDataMap;
		std::unordered_map<uint32_t, ChunkMemoryEntry> m_genericDataMap;
	};
}