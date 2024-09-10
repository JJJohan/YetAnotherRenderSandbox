#include "SMAAWeightsPass.hpp"
#include "../../Resources/IBuffer.hpp"
#include "../../Resources/IRenderImage.hpp"
#include "../../IDevice.hpp"
#include "../../Resources/ICommandBuffer.hpp"
#include "../../Resources/IMemoryBarriers.hpp"
#include "../../IResourceFactory.hpp"
#include "../../Renderer.hpp"
#include "../../../Core/Image.hpp"

namespace Engine::Rendering
{
	SMAAWeightsPass::SMAAWeightsPass()
		: IRenderPass("SMAAWeights", "SMAAWeights")
		, m_areaTexture(nullptr)
		, m_searchTexture(nullptr)
		, m_areaUploadBuffer(nullptr)
		, m_searchUploadBuffer(nullptr)
		, m_lookupTexturesUploaded(false)
	{
		m_imageInputInfos =
		{
			{"Edges", RenderPassImageInfo(AccessFlags::Read, Format::R8G8Unorm, {}, ImageLayout::ShaderReadOnly,
				MaterialStageFlags::FragmentShader, MaterialAccessFlags::ShaderRead)}
		};

		m_imageOutputInfos =
		{
			{"BlendedWeights", RenderPassImageInfo(AccessFlags::Write, Format::R8G8B8A8Unorm, {}, ImageLayout::ColorAttachment,
				MaterialStageFlags::ColorAttachmentOutput, MaterialAccessFlags::ColorAttachmentRead | MaterialAccessFlags::ColorAttachmentWrite)}
		};
	}

	void SMAAWeightsPass::ClearResources()
	{
		m_areaTexture.reset();
		m_searchTexture.reset();
		m_areaUploadBuffer.reset();
		m_searchUploadBuffer.reset();
		m_lookupTexturesUploaded = false;
		IRenderPass::ClearResources();
	}

	bool SMAAWeightsPass::CreateLookupTextures(const IDevice& device, const IResourceFactory& resourceFactory)
	{
		ImageUsageFlags usageFlags = ImageUsageFlags::Sampled | ImageUsageFlags::TransferDst;

		m_areaTexture = std::move(resourceFactory.CreateRenderImage());
		if (!m_areaTexture->Initialise("SMAAAreaTexture", device, ImageType::e2D, Format::R8G8B8A8Unorm, glm::uvec3(160, 560, 1), 1, 1,
			ImageTiling::Optimal, usageFlags, ImageAspectFlags::Color, MemoryUsage::AutoPreferDevice,
			AllocationCreateFlags::None, SharingMode::Exclusive))
		{
			Logger::Error("Failed to create SMAA area texture.");
			return false;
		}

		m_searchTexture = std::move(resourceFactory.CreateRenderImage());
		if (!m_searchTexture->Initialise("SMAASearchTexture", device, ImageType::e2D, Format::R8G8B8A8Unorm, glm::uvec3(64, 16, 1), 1, 1,
			ImageTiling::Optimal, usageFlags, ImageAspectFlags::Color, MemoryUsage::AutoPreferDevice,
			AllocationCreateFlags::None, SharingMode::Exclusive))
		{
			Logger::Error("Failed to create SMAA search texture.");
			return false;
		}

		return true;
	}

	bool CreateImageStagingBuffer(const IDevice& device, const IResourceFactory& resourceFactory,
		const ICommandBuffer& commandBuffer, const IRenderImage* destinationImage, const void* data, uint64_t size,
		std::unique_ptr<IBuffer>& outBuffer)
	{
		outBuffer = std::move(resourceFactory.CreateBuffer());
		if (!outBuffer->Initialise("imageStagingBuffer", device, size,
			BufferUsageFlags::TransferSrc, MemoryUsage::Auto,
			AllocationCreateFlags::HostAccessSequentialWrite | AllocationCreateFlags::Mapped,
			SharingMode::Exclusive))
		{
			return false;
		}

		if (!outBuffer->UpdateContents(data, 0, size))
			return false;

		outBuffer->CopyToImage(0, commandBuffer, *destinationImage);

		return true;
	}

	bool SMAAWeightsPass::UploadLookupTextureData(const IDevice& device, const IPhysicalDevice& physicalDevice,
		const IResourceFactory& resourceFactory, const ICommandBuffer& commandBuffer)
	{
		if (m_areaTexture == nullptr || m_searchTexture == nullptr)
		{
			return true; // Disposed during render pass, cancel resource upload.
		}

		Image areaImage;
		if (!areaImage.LoadFromFile("Textures/SMAAAreaTex.png"))
			return false;

		Image searchImage;
		if (!searchImage.LoadFromFile("Textures/SMAASearchTex.png"))
			return false;

		std::unique_ptr<IMemoryBarriers> memoryBarriers = std::move(resourceFactory.CreateMemoryBarriers());
		m_areaTexture->AppendImageLayoutTransition(commandBuffer, ImageLayout::TransferDst, *memoryBarriers);
		m_searchTexture->AppendImageLayoutTransition(commandBuffer, ImageLayout::TransferDst, *memoryBarriers);
		commandBuffer.MemoryBarrier(*memoryBarriers);
		memoryBarriers->Clear();

		const std::vector<uint8_t>& areaPixels = areaImage.GetPixels().front();
		if (!CreateImageStagingBuffer(device, resourceFactory, commandBuffer, m_areaTexture.get(), areaPixels.data(), areaPixels.size(), m_areaUploadBuffer))
			return false;

		const std::vector<uint8_t>& searchPixels = searchImage.GetPixels().front();
		if (!CreateImageStagingBuffer(device, resourceFactory, commandBuffer, m_searchTexture.get(), searchPixels.data(), searchPixels.size(), m_searchUploadBuffer))
			return false;

		m_areaTexture->AppendImageLayoutTransition(commandBuffer, ImageLayout::ShaderReadOnly, *memoryBarriers);
		m_searchTexture->AppendImageLayoutTransition(commandBuffer, ImageLayout::ShaderReadOnly, *memoryBarriers);
		m_areaUploadBuffer->AppendBufferMemoryBarrier(commandBuffer, MaterialStageFlags::Transfer, MaterialAccessFlags::MemoryWrite,
			MaterialStageFlags::FragmentShader, MaterialAccessFlags::ShaderRead, *memoryBarriers);
		m_searchUploadBuffer->AppendBufferMemoryBarrier(commandBuffer, MaterialStageFlags::Transfer, MaterialAccessFlags::MemoryWrite,
			MaterialStageFlags::FragmentShader, MaterialAccessFlags::ShaderRead, *memoryBarriers);
		commandBuffer.MemoryBarrier(*memoryBarriers);

		return true;
	}

	bool SMAAWeightsPass::Build(const Renderer& renderer,
		const std::unordered_map<std::string, IRenderImage*>& imageInputs,
		const std::unordered_map<std::string, IRenderImage*>& imageOutputs,
		const std::unordered_map<std::string, IBuffer*>& bufferInputs,
		const std::unordered_map<std::string, IBuffer*>& bufferOutputs)
	{
		ClearResources();

		const IDevice& device = renderer.GetDevice();
		const IResourceFactory& resourceFactory = renderer.GetResourceFactory();

		if (!CreateLookupTextures(device, resourceFactory))
			return false;

		m_colourAttachments.emplace_back(m_material->GetColourAttachmentInfo(0, imageOutputs.at("BlendedWeights"), AttachmentLoadOp::Clear));

		const std::vector<std::unique_ptr<IBuffer>>& frameInfoBuffers = renderer.GetFrameInfoBuffers();

		const IImageSampler& linearSampler = renderer.GetLinearSampler();
		const IImageView& edgesImageView = imageInputs.at("Edges")->GetView();

		if (!m_material->BindUniformBuffers(0, frameInfoBuffers) ||
			!m_material->BindCombinedImageSampler(1, linearSampler, edgesImageView, ImageLayout::ShaderReadOnly) ||
			!m_material->BindCombinedImageSampler(2, linearSampler, m_areaTexture->GetView(), ImageLayout::ShaderReadOnly) ||
			!m_material->BindCombinedImageSampler(3, linearSampler, m_searchTexture->GetView(), ImageLayout::ShaderReadOnly))
			return false;

		return IRenderNode::Build(renderer, imageInputs, imageOutputs, bufferInputs, bufferOutputs);
	}

	void SMAAWeightsPass::PreDraw(const Renderer& renderer, const ICommandBuffer& commandBuffer,
		const glm::uvec2& size, uint32_t frameIndex, const std::unordered_map<std::string, IRenderImage*>& imageInputs,
		const std::unordered_map<std::string, IRenderImage*>& imageOutputs)
	{
		if (!m_lookupTexturesUploaded)
		{
			if (m_areaUploadBuffer == nullptr)
			{
				const IDevice& device = renderer.GetDevice();
				const IPhysicalDevice& physicalDevice = renderer.GetPhysicalDevice();
				const IResourceFactory& resourceFactory = renderer.GetResourceFactory();
				UploadLookupTextureData(device, physicalDevice, resourceFactory, commandBuffer);
			}
		}
		else
		{
			if (m_areaUploadBuffer != nullptr)
			{
				m_areaUploadBuffer.reset();
				m_searchUploadBuffer.reset();
			}
		}
	}


	void SMAAWeightsPass::Draw(const Renderer& renderer, const ICommandBuffer& commandBuffer,
		const glm::uvec2& size, uint32_t frameIndex, uint32_t layerIndex)
	{
		m_material->BindMaterial(commandBuffer, BindPoint::Graphics, frameIndex);
		commandBuffer.Draw(3, 1, 0, 0);
	}
}