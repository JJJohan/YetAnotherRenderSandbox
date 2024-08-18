#pragma once

#include "../Types.hpp"

namespace Engine::Rendering
{
	class IRenderImage;
	class IBuffer;

	enum RenderNodeType
	{
		Pass,
		Compute,
		Resource
	};

	struct RenderPassResourceInfo
	{
		AccessFlags Access;
		uint32_t QueueFamilyIndex;
		RenderNodeType LastUsagePassType;
		uint32_t LastUsagePassStageIndex;

		RenderPassResourceInfo(AccessFlags accessFlags = AccessFlags::None)
			: Access(accessFlags)
			, QueueFamilyIndex(0)
			, LastUsagePassStageIndex(0)
			, LastUsagePassType(RenderNodeType::Pass)
		{
		}
	};

	struct RenderPassImageInfo : public RenderPassResourceInfo
	{
		Format Format;
		glm::uvec3 Dimensions;
		IRenderImage* Image;
		ImageLayout Layout;
		MaterialStageFlags StageFlags;
		MaterialAccessFlags MatAccessFlags;

		RenderPassImageInfo(AccessFlags accessFlags = AccessFlags::None, Engine::Rendering::Format format = Format::Undefined,
			const glm::uvec3& dimensions = {}, ImageLayout imageLayout = ImageLayout::Undefined, MaterialStageFlags stageFlags = MaterialStageFlags::None,
			MaterialAccessFlags materialAccessFlags = MaterialAccessFlags::None, IRenderImage* image = nullptr)
			: RenderPassResourceInfo(accessFlags)
			, Format(format)
			, Dimensions(dimensions)
			, Image(image)
			, Layout(imageLayout)
			, StageFlags(stageFlags)
			, MatAccessFlags(materialAccessFlags)
		{
		}
	};

	struct RenderPassBufferInfo : public RenderPassResourceInfo
	{
		IBuffer* Buffer;
		MaterialStageFlags StageFlags;
		MaterialAccessFlags MatAccessFlags;

		RenderPassBufferInfo(AccessFlags accessFlags = AccessFlags::None, MaterialStageFlags stageFlags = MaterialStageFlags::TopOfPipe,
			MaterialAccessFlags materialAccessFlags = MaterialAccessFlags::None, IBuffer* buffer = nullptr)
			: RenderPassResourceInfo(accessFlags)
			, Buffer(buffer)
			, StageFlags(stageFlags)
			, MatAccessFlags(materialAccessFlags)
		{
		}
	};
}