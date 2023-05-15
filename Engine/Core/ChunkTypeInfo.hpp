#pragma once

#include <stdint.h>

namespace Engine
{
	enum class ChunkResourceType
	{
		Generic,
		VertexBuffer,
		Image
	};

	enum class VertexBufferType
	{
		Positions,
		TextureCoordinates,
		Normals,
		Tangents,
		Bitangents
	};

	const uint64_t HeaderMagic = 0x3285130105;
	const uint16_t CurrentVersion = 1;

	struct ChunkHeader
	{
		const uint64_t Magic = HeaderMagic;
		const uint16_t Version = CurrentVersion;
		uint32_t ResourceCount;
	};

	struct ChunkResourceHeader
	{
		ChunkResourceType ResourceType;
		uint16_t Identifier;
		uint64_t ResourceSize;
	};

	struct VertexBufferHeader
	{
		VertexBufferType Type;
	};

	struct ImageHeader
	{
		uint32_t Width;
		uint32_t Height;
		bool Srgb;
	};
}