#include "DepthReductionPass.hpp"
#include "FrustumCullingPass.hpp"
#include "../Resources/IBuffer.hpp"
#include "../Resources/IImageSampler.hpp"
#include "../IResourceFactory.hpp"
#include "../IDevice.hpp"
#include "../Resources/ICommandBuffer.hpp"
#include "../Renderer.hpp"

using namespace Engine::Logging;

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
			{"Depth", RenderPassImageInfo(Format::D32Sfloat, true)}
		};

		m_imageOutputInfos =
		{
			{"OcclusionImage", RenderPassImageInfo(Format::R32Sfloat)}
		};
	}

	bool DepthReductionPass::CreateOcclusionImage(const IDevice& device, const IResourceFactory& resourceFactory)
	{
		ImageUsageFlags usageFlags = ImageUsageFlags::Storage | ImageUsageFlags::Sampled | ImageUsageFlags::TransferSrc;
		m_occlusionImage = std::move(resourceFactory.CreateRenderImage());
		if (!m_occlusionImage->Initialise("OcclusionImage", device, ImageType::e2D, Format::R32Sfloat, m_depthImage->GetDimensions(), m_depthPyramidLevels, 1,
			ImageTiling::Optimal, usageFlags, ImageAspectFlags::Color, MemoryUsage::AutoPreferDevice,
			AllocationCreateFlags::None, SharingMode::Exclusive))
		{
			Logger::Error("Failed to create TAA history image.");
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

	bool DepthReductionPass::Build(const Renderer& renderer,
		const std::unordered_map<const char*, IRenderImage*>& imageInputs,
		const std::unordered_map<const char*, IRenderImage*>& imageOutputs)
	{
		ClearResources();

		m_depthImage = imageInputs.at("Depth");
		const IDevice& device = renderer.GetDevice();
		const IResourceFactory& resourceFactory = renderer.GetResourceFactory();
		const glm::uvec3& extents = m_depthImage->GetDimensions();

		m_depthPyramidWidth = extents.x;
		m_depthPyramidHeight = extents.y;
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

		m_imageOutputInfos["OcclusionImage"] = RenderPassImageInfo(m_occlusionImage->GetFormat(), false, m_occlusionImage->GetDimensions(), m_occlusionImage.get());

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

		// TODO: Handle queue barriers between graphicsFamily and computeFamily to avoid issues with image layout.

		m_depthImage->TransitionImageLayoutExt(device, commandBuffer,
			MaterialStageFlags::LateFragmentTests, ImageLayout::DepthStencilAttachment, MaterialAccessFlags::DepthStencilAttachmentWrite,
			MaterialStageFlags::ComputeShader, ImageLayout::ShaderReadOnly, MaterialAccessFlags::ShaderRead);

		bool firstDraw = m_occlusionImage->GetLayout() == ImageLayout::Undefined;

		m_occlusionImage->TransitionImageLayoutExt(device, commandBuffer,
			firstDraw ? MaterialStageFlags::None : MaterialStageFlags::ComputeShader,
			m_occlusionImage->GetLayout(),
			firstDraw ? MaterialAccessFlags::None : MaterialAccessFlags::ShaderRead,
			MaterialStageFlags::ComputeShader, ImageLayout::General, 
			MaterialAccessFlags::ShaderWrite | MaterialAccessFlags::ShaderRead);

		m_material->BindMaterial(commandBuffer, BindPoint::Compute, frameIndex);

		for (uint32_t i = 0; i < m_depthPyramidLevels; ++i)
		{
			uint32_t levelWidth = std::max(1U, m_depthPyramidWidth >> i);
			uint32_t levelHeight = std::max(1U, m_depthPyramidHeight >> i);

			DimensionsAndIndex dimensionsAndIndex = { glm::vec2(levelWidth, levelHeight), i };
			commandBuffer.PushConstants(m_material, ShaderStageFlags::Compute, 0, sizeof(dimensionsAndIndex), reinterpret_cast<uint32_t*>(&dimensionsAndIndex));
			commandBuffer.Dispatch(getGroupCount(levelWidth, 32), getGroupCount(levelHeight, 32), 1);

			m_occlusionImage->TransitionImageLayoutExt(device, commandBuffer,
				MaterialStageFlags::ComputeShader, ImageLayout::General, MaterialAccessFlags::ShaderWrite,
				MaterialStageFlags::ComputeShader, ImageLayout::General, MaterialAccessFlags::ShaderRead);
		}

		m_depthImage->TransitionImageLayoutExt(device, commandBuffer,
			MaterialStageFlags::ComputeShader, ImageLayout::ShaderReadOnly, MaterialAccessFlags::ShaderRead,
			MaterialStageFlags::EarlyFragmentTests, ImageLayout::DepthStencilAttachment, MaterialAccessFlags::DepthStencilAttachmentRead | MaterialAccessFlags::DepthStencilAttachmentWrite);
	}
}