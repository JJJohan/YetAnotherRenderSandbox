#include "ResourceFactory.hpp"
#include "Buffer.hpp"
#include "ImageSampler.hpp"
#include "RenderImage.hpp"
#include "VulkanRenderer.hpp"

namespace Engine::Rendering::Vulkan
{
	ResourceFactory::ResourceFactory(VmaAllocator* allocator)
		: m_allocator(allocator)
	{
	}

	std::unique_ptr<IBuffer> ResourceFactory::CreateBuffer() const
	{
		return std::make_unique<Buffer>(*m_allocator);
	}

	std::unique_ptr<IRenderImage> ResourceFactory::CreateRenderImage() const
	{
		return std::make_unique<RenderImage>(*m_allocator);
	}

	std::unique_ptr<IImageSampler> ResourceFactory::CreateImageSampler() const
	{
		return std::make_unique<ImageSampler>();
	}
}