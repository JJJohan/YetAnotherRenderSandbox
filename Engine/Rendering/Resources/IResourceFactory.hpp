#pragma once

#include <memory>

namespace Engine::Rendering
{
	class IBuffer;
	class IImageSampler;
	class IRenderImage;

	class IResourceFactory
	{
	public:
		IResourceFactory() = default;
		virtual ~IResourceFactory() = default;
		virtual std::unique_ptr<IBuffer> CreateBuffer() const = 0;
		virtual std::unique_ptr<IRenderImage> CreateRenderImage() const = 0;
		virtual std::unique_ptr<IImageSampler> CreateImageSampler() const = 0;
	};
}