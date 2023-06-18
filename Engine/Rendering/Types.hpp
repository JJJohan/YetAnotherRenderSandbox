#pragma once
#include <stdint.h>

namespace Engine::Rendering
{
	enum class Format
	{
		Undefined,
		R8G8Unorm,
		R8G8B8A8Unorm,
		R16G16Sfloat,
		R16G16B16A16Sfloat,
		R32G32B32A32Sfloat,
		R16G16B16Sfloat,
		R32G32B32Sfloat,
		R32G32Sfloat,
		R32Sfloat,
		D32Sfloat,
		D32SfloatS8Uint, 
		D24UnormS8Uint,
		Bc5UnormBlock,
		Bc7SrgbBlock,
		Bc7UnormBlock,
		R8G8B8A8Srgb,
		R8G8B8Unorm,
		B8G8R8A8Unorm,
		Swapchain
	};

	enum class ImageAspectFlags
	{
		Color,
		Depth,
		Stencil,
		None
	};

	enum class ImageType
	{
		e1D,
		e2D,
		e3D
	};

	enum class IndexType
	{
		Uint16,
		Uint32
	};

	enum class ImageTiling
	{
		Optimal,
		Linear
	};

	enum class SharingMode
	{
		Exclusive,
		Concurrent
	};

	enum class ShaderStageFlags : uint32_t
	{
		Vertex = 1,
		TessellationControl = 2,
		TessellationEvaluation = 4,
		Geometry = 8,
		Fragment = 16,
		Compute = 32,
		Task = 64,
		Mesh = 128,
		Raygen = 256,
		AnyHit = 512,
		ClosestHit = 1024,
		Miss = 2048,
		Intersection = 4096,
		Callable = 8192
	};

	inline ShaderStageFlags& operator&=(ShaderStageFlags& a, ShaderStageFlags b)
	{
		a = static_cast<ShaderStageFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
		return a;
	}

	inline ShaderStageFlags& operator|=(ShaderStageFlags& a, ShaderStageFlags b)
	{
		a = static_cast<ShaderStageFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
		return a;
	}

	inline ShaderStageFlags operator|(ShaderStageFlags a, ShaderStageFlags b)
	{
		return a |= b;
	}

	inline ShaderStageFlags operator&(ShaderStageFlags a, ShaderStageFlags b)
	{
		return a &= b;
	}

	enum class Filter
	{
		Nearest,
		Linear
	};

	enum class SamplerMipmapMode
	{
		Nearest,
		Linear
	};

	enum class SamplerAddressMode
	{
		Repeat,
		MirroredRepeat,
		ClampToEdge,
		ClampToBorder,
		MirrorClampToEdge
	};

	enum class ImageLayout
	{
		Undefined,
		ColorAttachment,
		DepthStencilAttachment,
		ShaderReadOnly,
		TransferSrc,
		TransferDst,
		DepthAttachment,
		PresentSrc,
	};

	enum class ImageUsageFlags : uint32_t
	{
		TransferSrc = 0x00000001,
		TransferDst = 0x00000002,
		Sampled = 0x00000004,
		Storage = 0x00000008,
		ColorAttachment = 0x00000010,
		DepthStencilAttachment = 0x00000020,
		InputAttachment = 0x00000080
	};

	inline ImageUsageFlags& operator&=(ImageUsageFlags& a, ImageUsageFlags b)
	{
		a = static_cast<ImageUsageFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
		return a;
	}

	inline ImageUsageFlags& operator|=(ImageUsageFlags& a, ImageUsageFlags b)
	{
		a = static_cast<ImageUsageFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
		return a;
	}

	inline ImageUsageFlags operator|(ImageUsageFlags a, ImageUsageFlags b)
	{
		return a |= b;
	}

	inline ImageUsageFlags operator&(ImageUsageFlags a, ImageUsageFlags b)
	{
		return a &= b;
	}

    enum class MemoryUsage
    {
        Unknown,
        GPUOnly,
        CPUOnly,
        CPUToGPU,
        GPUToCPU,
        CPUCopy,
        GPULazilyAllocated,
        Auto,
        AutoPreferDevice,
        AutoPreferHost
    };

	enum class BufferUsageFlags : uint32_t
	{
		TransferSrc = 0x00000001,
		TransferDst = 0x00000002,
		UniformTexelBuffer = 0x00000004,
		StorageTexelBuffer = 0x00000008,
		UniformBuffer = 0x00000010,
		StorageBuffer = 0x00000020,
		IndexBuffer = 0x00000040,
		VertexBuffer = 0x00000080,
		IndirectBuffer = 0x00000100,
		ShaderDeviceAddress = 0x00020000,
		AccelerationStructureBuildInputReadOnly = 0x00080000,
		AccelerationStructureStorage = 0x00100000,
		ShaderBindingTable = 0x00000400,
		SamplerDescriptorBuffer = 0x00200000,
		ResourceDescriptorBuffer = 0x00400000,
		PushDescriptorsDescriptorBuffer = 0x04000000
	};

	inline BufferUsageFlags& operator&=(BufferUsageFlags& a, BufferUsageFlags b)
	{
		a = static_cast<BufferUsageFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
		return a;
	}

	inline BufferUsageFlags& operator|=(BufferUsageFlags& a, BufferUsageFlags b)
	{
		a = static_cast<BufferUsageFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
		return a;
	}

	inline BufferUsageFlags operator|(BufferUsageFlags a, BufferUsageFlags b)
	{
		return a |= b;
	}

	inline BufferUsageFlags operator&(BufferUsageFlags a, BufferUsageFlags b)
	{
		return a &= b;
	}

    enum class AllocationCreateFlags : uint32_t
    {
		None = 0,
		DedicatedMemory = 0x00000001,
		NeverAllocate = 0x00000002,
		Mapped = 0x00000004,
		UpperAddress = 0x00000040,
		DontBind = 0x00000080,
		WithinBudget = 0x00000100,
		CanAlias = 0x00000200,
		HostAccessSequentialWrite = 0x00000400,
		HostAccessRandom = 0x00000800,
		HostAccessAllowTransferInstead = 0x00001000,
		StrategyMinMemory = 0x00010000,
		StrategyMinTime = 0x00020000,
		StrategyMinOffset = 0x00040000,
		StrategyMask = StrategyMinMemory | StrategyMinTime | StrategyMinOffset
    };

	inline AllocationCreateFlags& operator&=(AllocationCreateFlags& a, AllocationCreateFlags b)
	{
		a = static_cast<AllocationCreateFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
		return a;
	}

	inline AllocationCreateFlags& operator|=(AllocationCreateFlags& a, AllocationCreateFlags b)
	{
		a = static_cast<AllocationCreateFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
		return a;
	}

	inline AllocationCreateFlags operator|(AllocationCreateFlags a, AllocationCreateFlags b)
	{
		return a |= b;
	}

	inline AllocationCreateFlags operator&(AllocationCreateFlags a, AllocationCreateFlags b)
	{
		return a &= b;
	}

	inline uint32_t GetSizeForFormat(Format format)
	{
		switch (format)
		{
		case Format::Undefined:
			return 0;
		case Format::R8G8Unorm:
			return 2;		
		case Format::R8G8B8A8Unorm:
		case Format::R16G16Sfloat:
		case Format::R32Sfloat:
		case Format::D32Sfloat:
		case Format::D24UnormS8Uint:
			return 4;
		case Format::D32SfloatS8Uint:
			return 5;
		case Format::R16G16B16Sfloat:
			return 6;
		case Format::R32G32Sfloat:
		case Format::R16G16B16A16Sfloat:
			return 8;
		case Format::R32G32B32Sfloat:
			return 12;
		case Format::R32G32B32A32Sfloat:
			return 16;
		default:
			throw;
		}
	}
}