#pragma once

#define STB_IMAGE_IMPLEMENTATION
#include "Image.hpp"
#include "Logging/Logger.hpp"
#include <filesystem>
#include <stb_image.h>

using namespace Engine::Logging;

namespace Engine
{
	Image::Image()
		: m_pixels()
		, m_size()
		, m_components(0)
	{
	}

	bool Image::LoadFromFile(const std::string& filePath)
	{
		if (!std::filesystem::exists(filePath))
		{
			Logger::Error("File {} does not exist.", filePath);
			return false;
		}

		uint8_t* image = stbi_load(filePath.c_str(), reinterpret_cast<int32_t*>(std::addressof(m_size.x)),
			reinterpret_cast<int32_t*>(&m_size.y), reinterpret_cast<int32_t*>(&m_components), STBI_rgb_alpha);

		if (image == nullptr)
		{
			Logger::Error("Failed to load image {}.", filePath);
			return false;
		}

		m_components = 4; // Alpha channel forced.

		uint32_t size = m_size.x * m_size.y * m_components;
		m_pixels.resize(size);
		memcpy(m_pixels.data(), image, size);

		stbi_image_free(image);

		return true;
	}

	bool Image::LoadFromMemory(const std::vector<uint8_t>& memory)
	{
		Logger::Error("Not implemented.");
		return false;
	}

	const std::vector<uint8_t>& Image::GetPixels() const
	{
		return m_pixels;
	}

	const glm::uvec2& Image::GetSize() const
	{
		return m_size;
	}

	uint32_t Image::GetComponentCount() const
	{
		return m_components;
	}
}