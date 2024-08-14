#pragma once

#include "../Types.hpp"

namespace Engine::Rendering
{
	class IRenderImage;
	class IBuffer;

	struct RenderPassResourceInfo
	{
		AccessFlags Access;

		RenderPassResourceInfo(AccessFlags accessFlags = AccessFlags::None)
			: Access(accessFlags)
		{
		}
	};

	struct RenderPassImageInfo : public RenderPassResourceInfo
	{
		Format Format;
		glm::uvec3 Dimensions;
		IRenderImage* Image;

		RenderPassImageInfo(AccessFlags accessFlags = AccessFlags::None, Engine::Rendering::Format format = Format::Undefined, const glm::uvec3& dimensions = {}, IRenderImage* image = nullptr)
			: RenderPassResourceInfo(accessFlags)
			, Format(format)
			, Dimensions(dimensions)
			, Image(image)
		{
		}
	};

	struct RenderPassBufferInfo : public RenderPassResourceInfo
	{
		IBuffer* Buffer;

		RenderPassBufferInfo(AccessFlags accessFlags = AccessFlags::None, IBuffer* buffer = nullptr)
			: RenderPassResourceInfo(accessFlags)
			, Buffer(buffer)
		{
		}
	};
}