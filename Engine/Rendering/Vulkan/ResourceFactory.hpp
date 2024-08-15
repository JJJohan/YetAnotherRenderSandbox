#pragma once

#include "../IResourceFactory.hpp"
#include <vma/vk_mem_alloc.h>

namespace Engine::Rendering::Vulkan
{
	class ResourceFactory : public IResourceFactory
	{
	public:
		ResourceFactory(VmaAllocator* allocator);
		virtual std::unique_ptr<IBuffer> CreateBuffer() const override;
		virtual std::unique_ptr<IRenderImage> CreateRenderImage() const override;
		virtual std::unique_ptr<IImageSampler> CreateImageSampler() const override;
		virtual std::unique_ptr<ICommandPool> CreateCommandPool() const override;
		virtual std::unique_ptr<ISemaphore> CreateGraphicsSemaphore() const override;
		virtual std::unique_ptr<IImageMemoryBarriers> CreateImageMemoryBarriers() const override;

	private:
		VmaAllocator* m_allocator;
	};
}