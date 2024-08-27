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
		MaterialStageFlags StageFlags;
		MaterialAccessFlags MatAccessFlags;

		RenderPassResourceInfo(AccessFlags accessFlags = AccessFlags::None, MaterialStageFlags stageFlags = MaterialStageFlags::None, MaterialAccessFlags materialAccessFlags = MaterialAccessFlags::None)
			: Access(accessFlags)
			, QueueFamilyIndex(0)
			, LastUsagePassStageIndex(0)
			, LastUsagePassType(RenderNodeType::Pass)
			, StageFlags(stageFlags)
			, MatAccessFlags(materialAccessFlags)
		{
		}
	};

	struct RenderPassImageInfo : public RenderPassResourceInfo
	{
		Format Format;
		glm::uvec3 Dimensions;
		IRenderImage* Image;
		ImageLayout Layout;

		RenderPassImageInfo(AccessFlags accessFlags = AccessFlags::None, Engine::Rendering::Format format = Format::Undefined,
			const glm::uvec3& dimensions = {}, ImageLayout imageLayout = ImageLayout::Undefined, MaterialStageFlags stageFlags = MaterialStageFlags::None,
			MaterialAccessFlags materialAccessFlags = MaterialAccessFlags::None, IRenderImage* image = nullptr)
			: RenderPassResourceInfo(accessFlags, stageFlags, materialAccessFlags)
			, Format(format)
			, Dimensions(dimensions)
			, Image(image)
			, Layout(imageLayout)
		{
		}
	};

	struct RenderPassBufferInfo : public RenderPassResourceInfo
	{
		IBuffer* Buffer;

		RenderPassBufferInfo(AccessFlags accessFlags = AccessFlags::None, MaterialStageFlags stageFlags = MaterialStageFlags::TopOfPipe,
			MaterialAccessFlags materialAccessFlags = MaterialAccessFlags::None, IBuffer* buffer = nullptr)
			: RenderPassResourceInfo(accessFlags, stageFlags, materialAccessFlags)
			, Buffer(buffer)
		{
		}
	};
}