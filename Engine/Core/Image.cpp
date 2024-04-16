#include "Image.hpp"
#include "Logger.hpp"
#include "Utilities.hpp"
#include "AsyncData.hpp"
#include "Hash.hpp"
#include <filesystem>
#include <fstream>
#include <bc7enc.h>
#include <rgbcx.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>

#define WUFFS_IMPLEMENTATION
#define WUFFS_CONFIG__STATIC_FUNCTIONS
#include <wuffs.h>

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

		wuffs_aux::DecodeImageCallbacks callbacks{};
		wuffs_aux::sync_io::MemoryInput input(memory, size);
		wuffs_aux::DecodeImageResult res = wuffs_aux::DecodeImage(callbacks, input);

		if (!res.error_message.empty())
		{
			Logger::Error("Failed to parse image file: {}", res.error_message);
			return false;
		}

		m_size.x = res.pixbuf.pixcfg.width();
		m_size.y = res.pixbuf.pixcfg.height();
		m_components = 4; // Source data will always be forced to 4 components, compression may reduce component count.

		std::vector<uint8_t>& pixels = m_mipMaps.emplace_back(std::vector<uint8_t>(res.pixbuf.pixcfg.pixbuf_len()));
		memcpy(pixels.data(), res.pixbuf_mem_owner.get(), res.pixbuf.pixcfg.pixbuf_len());

		// ARGB -> RGBA
		uint32_t pixelCount = static_cast<uint32_t>(pixels.size()) / sizeof(uint32_t);
		uint32_t* pixelsRgba = reinterpret_cast<uint32_t*>(pixels.data());
		for (uint32_t i = 0; i < pixelCount; ++i)
		{
			uint32_t p = pixelsRgba[i];
			pixelsRgba[i] = ((p & 0x00FF0000) >> 16) | ((p & 0x0000FF00)) | ((p & 0x000000FF) << 16) | ((p & 0xFF000000));
		}

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

			stbir_resize_uint8_linear(
				inputMips[mip - 1].data(), srcWidth, srcHeight, srcPitchX * m_components,
				inputMips[mip].data(), mipWidth, mipHeight, mipPitchX * m_components,
				static_cast<stbir_pixel_layout>(m_components));
		}

		bc7enc_compress_block_params params;
		bool bc5Compress = false;
		uint8_t bc5_c1 = 0;
		uint8_t bc5_c2 = 1;
		if (m_compressed)
		{
			bc5Compress = IsNormalMap() || IsMetallicRoughnessMap();
			if (!bc5Compress)
			{
				bc7enc_compress_block_params_init(&params);
			}
			else
			{
				if (IsMetallicRoughnessMap())
				{
					bc5_c1 = 2;
					bc5_c2 = 1;
				}
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
							rgbcx::encode_bc5(outputData + dstOffset, blockData, bc5_c1, bc5_c2, m_components);
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