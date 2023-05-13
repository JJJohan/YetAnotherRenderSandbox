#pragma once

#include <stdint.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include "Macros.hpp"

namespace Engine
{
	class Image
	{
	public:
		EXPORT Image();
		EXPORT Image(const glm::uvec2& dimensions, uint32_t components, const std::vector<uint8_t>& pixels, bool srgb = true);
		EXPORT bool LoadFromFile(const std::string& filePath, bool srgb = true);
		EXPORT bool LoadFromMemory(const uint8_t* memory, size_t size, bool srgb = true);

		EXPORT const std::vector<uint8_t>& GetPixels() const;
		EXPORT const glm::uvec2& GetSize() const;
		EXPORT bool IsSRGB() const;
		EXPORT uint32_t GetComponentCount() const;
		uint64_t GetHash() const;

	private:
		bool m_srgb;
		std::vector<uint8_t> m_pixels;
		glm::uvec2 m_size;
		uint32_t m_components;
		uint64_t m_hash;
	};
}