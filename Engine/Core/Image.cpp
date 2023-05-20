#include "Image.hpp"
#include "Logging/Logger.hpp"
#include "AsyncData.hpp"
#include "Hash.hpp"
#include <filesystem>
#include <fstream>
#include <bc7enc.h>
#include <rgbcx.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize.h>

#define WUFFS_IMPLEMENTATION
#define WUFFS_CONFIG__STATIC_FUNCTIONS
#include <wuffs.h>

using namespace Engine::Logging;

namespace Engine
{
	bool Image::m_compressInit = false;

	Image::Image()
		: m_mipMaps()
		, m_size()
		, m_components(0)
		, m_hash(0)
		, m_imageFlags(ImageFlags::SRGB)
		, m_compressed(false)
	{
	}

	Image::Image(const glm::uvec2& dimensions, uint32_t components, const std::vector<uint8_t>& pixels, ImageFlags imageFlags)
		: m_mipMaps({ pixels })
		, m_size(dimensions)
		, m_components(components)
		, m_hash(Hash::CalculateHash(pixels))
		, m_imageFlags(imageFlags)
		, m_compressed(false)
	{
	}

	void Image::CompressInit()
	{
		if (m_compressInit) return;

		rgbcx::init();
		bc7enc_compress_block_init();
		m_compressInit = true;
	}

	bool Image::LoadFromFile(const std::string& filePath, ImageFlags imageFlags)
	{
		if (!std::filesystem::exists(filePath))
		{
			Logger::Error("File {} does not exist.", filePath);
			return false;
		}

		std::ifstream input(filePath, std::ios::binary);
		std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(input), {});
		input.close();

		if (!LoadFromMemory(buffer.data(), buffer.size(), imageFlags))
		{
			Logger::Error("Failed to load image {}.", filePath);
			return false;
		}

		return true;
	}

	bool Image::LoadFromMemory(const uint8_t* memory, size_t size, ImageFlags imageFlags)
	{
		m_imageFlags = imageFlags;

		wuffs_png__decoder* dec = wuffs_png__decoder__alloc();

		wuffs_png__decoder__set_quirk(dec, WUFFS_BASE__QUIRK_IGNORE_CHECKSUM, true);

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
		m_components = 4; // Source data will always be forced to 4 components, compression may reduce component count.

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

		std::vector<uint8_t>& pixels = m_mipMaps.emplace_back(std::vector<uint8_t>(m_size.x * m_size.y * m_components));
		wuffs_base__slice_u8 pixbuf_slice = wuffs_base__make_slice_u8(pixels.data(), pixels.size());

		wuffs_base__pixel_buffer pb;
		status = wuffs_base__pixel_buffer__set_from_slice(&pb, &ic.pixcfg, pixbuf_slice);

		if (status.repr)
		{
			free(workbuf_slice.ptr);
			free(dec);
			m_mipMaps.clear();
			return false;
		}

		status = wuffs_png__decoder__decode_frame(dec, &pb, &src, WUFFS_BASE__PIXEL_BLEND__SRC, workbuf_slice, NULL);

		if (status.repr)
		{
			free(workbuf_slice.ptr);
			free(dec);
			m_mipMaps.clear();
			return false;
		}

		free(workbuf_slice.ptr);
		free(dec);

		m_hash = Hash::CalculateHash(m_mipMaps.front());

		return true;
	}

	inline void Image::GetBlock(uint8_t* block, const uint8_t* pixels, uint32_t width, size_t pixelOffset)
	{
		for (uint32_t i = 0; i < 4; ++i)
			memcpy(block + i * m_components * 4, pixels + pixelOffset + width * m_components * i, m_components * 4);
	}

	bool Image::Optimise(bool compress, bool generateMipMaps, const AsyncData* asyncData)
	{
		const uint32_t minMipSize = compress ? 4 : 1;
		if (compress && (m_size.x < minMipSize || m_size.y < minMipSize))
			compress = false;

		m_compressed = compress;

		// Generate the input mipmap chain(s). At every level, the input
		// width and height must be padded up to a multiple of the output
		// block dimensions.

		const uint32_t bytesPerBlock = m_compressed ? 16 : 4;
		const uint32_t blockDimX = m_compressed ? 4 : 1;
		const uint32_t blockDimY = m_compressed ? 4 : 1;

		uint32_t mipWidth = m_size.x;
		uint32_t mipHeight = m_size.y;
		uint32_t mipPitchX = ((mipWidth + blockDimX - 1) / blockDimX) * blockDimX;
		uint32_t mipPitchY = ((mipHeight + blockDimY - 1) / blockDimY) * blockDimY;

		// Determine mip chain properties
		uint32_t mipLevels = 1;
		if (generateMipMaps)
		{
			uint32_t mipWidth = m_size.x, mipHeight = m_size.y;
			while (mipWidth > minMipSize || mipHeight > minMipSize)
			{
				++mipLevels;
				mipWidth = std::max(1u, mipWidth / 2);
				mipHeight = std::max(1u, mipHeight / 2);
			}
		}

		std::vector<std::vector<uint8_t>> inputMips(mipLevels);
		m_mipMaps.resize(mipLevels);

		// Populate padded input mip level 0
		inputMips[0].resize(mipPitchX * mipPitchY * m_components);
		uint8_t* frontInputMip = inputMips.front().data();
		uint8_t* frontOutputMip = m_mipMaps.front().data();
		for (uint32_t y = 0; y < m_size.y; ++y)
		{
			memcpy(frontInputMip + y * mipWidth * m_components,
				frontOutputMip + y * m_size.x * m_components,
				m_size.x * m_components);
		}

		// Generate additional mips, if necessary
		for (uint32_t mip = 1; mip < mipLevels; ++mip)
		{
			uint32_t srcWidth = mipWidth;
			uint32_t srcHeight = mipHeight;
			uint32_t srcPitchX = mipPitchX;
			mipWidth = std::max(1u, mipWidth / 2);
			mipHeight = std::max(1u, mipHeight / 2);
			mipPitchX = ((mipWidth + blockDimX - 1) / blockDimX) * blockDimX;
			mipPitchY = ((mipHeight + blockDimY - 1) / blockDimY) * blockDimY;
			inputMips[mip].resize(mipPitchX * mipPitchY * m_components);

			stbir_resize_uint8(
				inputMips[mip - 1].data(), srcWidth, srcHeight, srcPitchX * m_components,
				inputMips[mip].data(), mipWidth, mipHeight, mipPitchX * m_components,
				m_components);
		}

		bc7enc_compress_block_params params;
		bool bc5Compress = false;
		if (m_compressed)
		{
			bc5Compress = IsNormalMap() || IsMetallicRoughnessMap();
			if (!bc5Compress)
			{
				bc7enc_compress_block_params_init(&params);
			}
		}

		// Generate output mip chain
		mipWidth = m_size.x;
		mipHeight = m_size.y;
		for (uint32_t mip = 0; mip < mipLevels; ++mip)
		{
			if (!m_compressed)
			{
				m_mipMaps[mip] = inputMips[mip];
			}
			else
			{
				uint32_t blockCount = (mipWidth / blockDimX) * (mipHeight / blockDimY);
				uint32_t totalSize = blockCount * bytesPerBlock;
				const uint32_t stride = mipWidth * m_components;
				m_mipMaps[mip].resize(totalSize);

				const uint8_t* inputData = inputMips[mip].data();
				uint8_t* outputData = m_mipMaps[mip].data();
				size_t size = inputMips[mip].size();

				uint32_t dstOffset = 0;
				std::vector<uint8_t> block(4 * 4 * m_components);
				uint8_t* blockData = block.data();
				for (size_t srcOffset = 0; srcOffset < size; srcOffset += stride * 4)
				{
					for (size_t strideOffset = 0; strideOffset < stride; strideOffset += 4 * m_components)
					{
						GetBlock(blockData, inputData, mipWidth, srcOffset + strideOffset);

						if (bc5Compress)
							rgbcx::encode_bc5(outputData + dstOffset, blockData, 0U, 1U, m_components);
						else
							bc7enc_compress_block(outputData + dstOffset, blockData, &params);
						dstOffset += bytesPerBlock;
					}
				}
			}

			if (asyncData != nullptr && asyncData->State == AsyncState::Cancelled)
			{
				return false;
			}

			mipWidth = std::max(1u, mipWidth / 2);
			mipHeight = std::max(1u, mipHeight / 2);
		}

		if (m_compressed && bc5Compress)
		{
			m_components = 2;
		}

		return true;
	}
}