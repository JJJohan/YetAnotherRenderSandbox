#include "DepthReductionPass.hpp"
#include "FrustumCullingPass.hpp"
#include "../Resources/IBuffer.hpp"
#include "../Resources/IImageSampler.hpp"
#include "../IResourceFactory.hpp"
#include "../IDevice.hpp"
#include "../Resources/ICommandBuffer.hpp"
#include "../Resources/IMemoryBarriers.hpp"
#include "../Renderer.hpp"

namespace Engine::Rendering
{
	DepthReductionPass::DepthReductionPass(FrustumCullingPass& frustumCullingPass)
		: IComputePass("DepthReduction", "DepthReduction")
		, m_occlusionImage(nullptr)
		, m_depthPyramidWidth(0)
		, m_depthPyramidHeight(0)
		, m_depthPyramidLevels(0)
		, m_occlusionMipViews()
		, m_frustumCullingPass(frustumCullingPass)
		, m_depthImage(nullptr)
	{
		m_imageInputInfos =
		{
			{"Depth", RenderPassImageInfo(AccessFlags::Read, Format::D32Sfloat, {}, ImageLayout::ShaderReadOnly,
				MaterialStageFlags::ComputeShader, MaterialAccessFlags::ShaderRead)}
		};

		m_imageOutputInfos =
		{
			{"OcclusionImage", RenderPassImageInfo(AccessFlags::Write, Format::R32Sfloat, {}, ImageLayout::General,
				MaterialStageFlags::ComputeShader, MaterialAccessFlags::ShaderWrite | MaterialAccessFlags::ShaderRead)}
		};
	}

	bool DepthReductionPass::CreateOcclusionImage(const IDevice& device, const IResourceFactory& resourceFactory)
	{
		glm::uvec3 dimensions(m_depthPyramidWidth, m_depthPyramidHeight, 1);

		ImageUsageFlags usageFlags = ImageUsageFlags::Storage | ImageUsageFlags::Sampled | ImageUsageFlags::TransferSrc;
		m_occlusionImage = std::move(resourceFactory.CreateRenderImage());
		if (!m_occlusionImage->Initialise("OcclusionImage", device, ImageType::e2D, Format::R32Sfloat, dimensions, m_depthPyramidLevels, 1,
			ImageTiling::Optimal, usageFlags, ImageAspectFlags::Color, MemoryUsage::AutoPreferDevice,
			AllocationCreateFlags::None, SharingMode::Exclusive))
		{
			Logger::Error("Failed to create occlusion image.");
			return false;
		}

		return true;
	}

	inline static uint32_t getImageMipLevels(uint32_t width, uint32_t height)
	{
		uint32_t result = 1;

		while (width > 1 || height > 1)
		{
			++result;
			width /= 2;
			height /= 2;
		}

		return result;
	}

	static inline uint32_t previousPow2(uint32_t v)
	{
		uint32_t r = 1;

		while (r * 2 < v)
			r *= 2;

		return r;
	}

	bool DepthReductionPass::Build(const Renderer& renderer,
		const std::unordered_map<std::string, IRenderImage*>& imageInputs,
		const std::unordered_map<std::string, IRenderImage*>& imageOutputs,
		const std::unordered_map<std::string, IBuffer*>& bufferInputs,
		const std::unordered_map<std::string, IBuffer*>& bufferOutputs)
	{
		ClearResources();

		m_depthImage = imageInputs.at("Depth");
		m_imageInputInfos.at("Depth").Image = m_depthImage;
		const IDevice& device = renderer.GetDevice();
		const IResourceFactory& resourceFactory = renderer.GetResourceFactory();
		const glm::uvec3& extents = m_depthImage->GetDimensions();

		m_depthPyramidWidth = previousPow2(extents.x);
		m_depthPyramidHeight = previousPow2(extents.y);
		m_depthPyramidLevels = getImageMipLevels(m_depthPyramidWidth, m_depthPyramidHeight);

		if (!CreateOcclusionImage(device, resourceFactory))
		{
			return false;
		}

		if (!m_frustumCullingPass.FrustumPassBuild(renderer, m_occlusionImage.get()))
		{
			Logger::Error("Failed to perform separate frustum pass build.");
			return false;
		}

		m_occlusionMipViews.resize(m_depthPyramidLevels);
		for (uint32_t i = 0; i < m_depthPyramidLevels; ++i)
		{
			std::string name = std::format("OcclusionMipView{}", i);
			if (!m_occlusionImage->CreateView(name.c_str(), device, i, ImageAspectFlags::Color, m_occlusionMipViews[i]))
			{
				return false;
			}
		}

		std::vector<const IImageSampler*> sourceSamplers;
		std::vector<ImageLayout> sourceLayouts;
		std::vector<const IImageView*> sourceImageViews;
		std::vector<const IImageView*> destImageViews;
		sourceSamplers.resize(m_depthPyramidLevels);
		sourceImageViews.resize(m_depthPyramidLevels);
		sourceLayouts.resize(m_depthPyramidLevels);
		destImageViews.resize(m_depthPyramidLevels);
		for (uint32_t i = 0; i < m_depthPyramidLevels; ++i)
		{
			const IImageView* sourceView = i == 0 ? &m_depthImage->GetView() : m_occlusionMipViews[i - 1].get();
			sourceSamplers[i] = &renderer.GetReductionSampler();
			sourceImageViews[i] = sourceView;
			sourceLayouts[i] = i == 0 ? ImageLayout::ShaderReadOnly : ImageLayout::General;
			destImageViews[i] = m_occlusionMipViews[i].get();
		}

		if (!m_material->BindCombinedImageSamplers(0, sourceSamplers, sourceImageViews, sourceLayouts) ||
			!m_material->BindStorageImages(1, destImageViews))
			return false;

		m_imageOutputInfos["OcclusionImage"] = RenderPassImageInfo(AccessFlags::Write, m_occlusionImage->GetFormat(), m_occlusionImage->GetDimensions(),
			ImageLayout::General, MaterialStageFlags::ComputeShader,
			MaterialAccessFlags::ShaderWrite | MaterialAccessFlags::ShaderRead,
			m_occlusionImage.get());

		return true;
	}

	inline static uint32_t getGroupCount(uint32_t threadCount, uint32_t localSize)
	{
		return (threadCount + localSize - 1) / localSize;
	}

	struct alignas(16) DimensionsAndIndex
	{
		glm::vec2 Dimensions;
		uint32_t Index;
	};

	void DepthReductionPass::Dispatch(const Renderer& renderer, const ICommandBuffer& commandBuffer,
		uint32_t frameIndex)
	{
		const IDevice& device = renderer.GetDevice();

		std::unique_ptr<IMemoryBarriers> memoryBarriers = std::move(renderer.GetResourceFactory().CreateMemoryBarriers());

		if (m_occlusionImage->GetLayout() == ImageLayout::Undefined)
		{
			m_occlusionImage->AppendImageLayoutTransitionExt(commandBuffer,
				MaterialStageFlags::Transfer, ImageLayout::TransferDst, MaterialAccessFlags::TransferWrite, *memoryBarriers);
			commandBuffer.MemoryBarrier(*memoryBarriers);
			memoryBarriers->Clear();

			commandBuffer.ClearColourImage(*m_occlusionImage, Engine::Colour());
		}

		m_occlusionImage->AppendImageLayoutTransitionExt(commandBuffer,
			MaterialStageFlags::ComputeShader, ImageLayout::General, MaterialAccessFlags::ShaderRead, *memoryBarriers);

		m_material->BindMaterial(commandBuffer, BindPoint::Compute, frameIndex);

		for (uint32_t i = 0; i < m_depthPyramidLevels; ++i)
		{
			uint32_t levelWidth = std::max(1U, m_depthPyramidWidth >> i);
			uint32_t levelHeight = std::max(1U, m_depthPyramidHeight >> i);

			DimensionsAndIndex dimensionsAndIndex = { glm::vec2(levelWidth, levelHeight), i };
			commandBuffer.PushConstants(m_material, ShaderStageFlags::Compute, 0, sizeof(dimensionsAndIndex), reinterpret_cast<uint32_t*>(&dimensionsAndIndex));
			commandBuffer.Dispatch(getGroupCount(levelWidth, 32), getGroupCount(levelHeight, 32), 1);
			commandBuffer.MemoryBarrier(*memoryBarriers);
		}
	}
}