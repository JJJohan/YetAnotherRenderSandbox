#include "Image.hpp"
#include "Logging/Logger.hpp"
#include "Hash.hpp"
#include <filesystem>
#include <fstream>

#define WUFFS_IMPLEMENTATION
#define WUFFS_CONFIG__STATIC_FUNCTIONS
#include <wuffs.h>

using namespace Engine::Logging;

namespace Engine
{
	Image::Image()
		: m_pixels()
		, m_size()
		, m_components(0)
		, m_hash(0)
	{
	}

	Image::Image(const glm::uvec2& dimensions, uint32_t components, const std::vector<uint8_t>& pixels)
		: m_pixels(pixels)
		, m_size(dimensions)
		, m_components(components)
		, m_hash(Hash::CalculateHash(pixels))
	{
	}

	bool Image::LoadFromFile(const std::string& filePath)
	{
		if (!std::filesystem::exists(filePath))
		{
			Logger::Error("File {} does not exist.", filePath);
			return false;
		}

		std::ifstream input(filePath, std::ios::binary);
		std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(input), {});
		input.close();

		if (LoadFromMemory(buffer.data(), buffer.size()))
		{
			Logger::Error("Failed to load image {}.", filePath);
			return false;
		}

		return true;
	}

	bool Image::LoadFromMemory(const uint8_t* memory, size_t size)
	{
		wuffs_png__decoder* dec = wuffs_png__decoder__alloc();

		wuffs_png__decoder__set_quirk_enabled(dec, WUFFS_BASE__QUIRK_IGNORE_CHECKSUM, true);

		wuffs_base__image_config ic;
		wuffs_base__io_buffer src = wuffs_base__ptr_u8__reader(const_cast<uint8_t*>(memory), size, true);
		wuffs_base__status status = wuffs_png__decoder__decode_image_config(dec, &ic, &src);

		if (status.repr)
		{
			free(dec);
			return false;
		}

		m_size.x = wuffs_base__pixel_config__width(&ic.pixcfg);
		m_size.y = wuffs_base__pixel_config__height(&ic.pixcfg);

		wuffs_base__pixel_config__set(&ic.pixcfg, WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL, WUFFS_BASE__PIXEL_SUBSAMPLING__NONE, m_size.x, m_size.y);

		uint64_t workbuf_len = wuffs_png__decoder__workbuf_len(dec).max_incl;
		if (workbuf_len > SIZE_MAX)
		{
			free(dec);
			return false;
		}

		wuffs_base__slice_u8 workbuf_slice = wuffs_base__make_slice_u8((uint8_t*)malloc((size_t)workbuf_len), (size_t)workbuf_len);
		if (!workbuf_slice.ptr)
		{
			free(dec);
			return false;
		}

		m_components = 4; // Alpha channel forced.
		m_pixels.resize(m_size.x * m_size.y * m_components);

		wuffs_base__slice_u8 pixbuf_slice = wuffs_base__make_slice_u8(m_pixels.data(), m_pixels.size());

		wuffs_base__pixel_buffer pb;
		status = wuffs_base__pixel_buffer__set_from_slice(&pb, &ic.pixcfg, pixbuf_slice);

		if (status.repr)
		{
			free(workbuf_slice.ptr);
			free(dec);
			m_pixels.clear();
			return false;
		}

		status = wuffs_png__decoder__decode_frame(dec, &pb, &src, WUFFS_BASE__PIXEL_BLEND__SRC, workbuf_slice, NULL);

		if (status.repr)
		{
			free(workbuf_slice.ptr);
			free(dec);
			m_pixels.clear();
			return false;
		}

		free(workbuf_slice.ptr);
		free(dec);

		m_hash = Hash::CalculateHash(m_pixels);

		return true;
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

	uint64_t Image::GetHash() const
	{
		return m_hash;
	}
}