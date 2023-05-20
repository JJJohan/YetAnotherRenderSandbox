#pragma once

#include <stdint.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include "Macros.hpp"

namespace Engine
{
	class AsyncData;

	enum class ImageFlags
	{
		SRGB = 1 << 0,
		NormalMap = 1 << 1,
		MetallicRoughnessMap = 1 << 2
	};

	inline ImageFlags& operator&=(ImageFlags& a, ImageFlags b)
	{
		a = static_cast<ImageFlags>(static_cast<int32_t>(a) & static_cast<int32_t>(b));
		return a;
	}

	inline ImageFlags& operator|=(ImageFlags& a, ImageFlags b)
	{
		a = static_cast<ImageFlags>(static_cast<int32_t>(a) | static_cast<int32_t>(b));
		return a;;
	}

	inline ImageFlags operator|(ImageFlags a, ImageFlags b)
	{
		return a |= b;
	}

	inline ImageFlags operator&(ImageFlags a, ImageFlags b)
	{
		return a &= b;
	}

	class Image
	{
	public:
		EXPORT Image();
		EXPORT Image(const glm::uvec2& dimensions, uint32_t components, const std::vector<uint8_t>& pixels, ImageFlags flags = ImageFlags::SRGB);
		EXPORT bool LoadFromFile(const std::string& filePath, ImageFlags flags = ImageFlags::SRGB);
		EXPORT bool LoadFromMemory(const uint8_t* memory, size_t size, ImageFlags flags = ImageFlags::SRGB);

		EXPORT bool Optimise(bool compress, bool generateMipMaps, const AsyncData* asyncData = nullptr);

		inline const std::vector<std::vector<uint8_t>>& GetPixels() const
		{
			return m_mipMaps;
		}

		inline const glm::uvec2& GetSize() const
		{
			return m_size;
		}

		inline bool IsSRGB() const
		{
			return (m_imageFlags & ImageFlags::SRGB) == ImageFlags::SRGB;
		}

		inline bool IsNormalMap() const
		{
			return (m_imageFlags & ImageFlags::NormalMap) == ImageFlags::NormalMap;
		}

		inline bool IsMetallicRoughnessMap() const
		{
			return (m_imageFlags & ImageFlags::MetallicRoughnessMap) == ImageFlags::MetallicRoughnessMap;
		}

		inline bool IsCompressed() const
		{
			return m_compressed;
		}

		inline uint32_t GetComponentCount() const
		{
			return m_components;
		}

		inline uint64_t GetHash() const
		{
			return m_hash;
		}

		static void CompressInit();

	private:
		inline void GetBlock(uint8_t* block, const uint8_t* pixels, uint32_t width, size_t pixelOffset);

		ImageFlags m_imageFlags;
		bool m_compressed;
		static bool m_compressInit;
		std::vector<std::vector<uint8_t>> m_mipMaps;
		glm::uvec2 m_size;
		uint32_t m_components;
		uint64_t m_hash;
	};
}