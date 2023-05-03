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
		EXPORT Image(const glm::uvec2& dimensions, uint32_t components, const std::vector<uint8_t>& pixels);
		EXPORT bool LoadFromFile(const std::string& filePath);
		EXPORT bool LoadFromMemory(const std::vector<uint8_t>& memory);

		EXPORT const std::vector<uint8_t>& GetPixels() const;
		EXPORT const glm::uvec2& GetSize() const;
		EXPORT uint32_t GetComponentCount() const;

	private:
		std::vector<uint8_t> m_pixels;
		glm::uvec2 m_size;
		uint32_t m_components;
	};
}