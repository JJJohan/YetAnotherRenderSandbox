#include "FrustumCullingPass.hpp"
#include "DepthReductionPass.hpp"
#include "../Resources/IBuffer.hpp"
#include "../IResourceFactory.hpp"
#include "../IDevice.hpp"
#include "../Resources/ICommandBuffer.hpp"
#include "../Resources/IMemoryBarriers.hpp"
#include "../Resources/GeometryBatch.hpp"
#include "../Renderer.hpp"

namespace Engine::Rendering
{
	FrustumCullingPass::FrustumCullingPass(const GeometryBatch& sceneGeometryBatch)
		: IComputePass("FrustumCulling", "FrustumCulling")
		, m_sceneGeometryBatch(sceneGeometryBatch)
		, m_built(false)
		, m_dispatchSize(0)
		, m_mode(CullingMode::FrustumAndOcclusion)
		, m_drawCullData()
		, m_occlusionImage(nullptr)
		, m_indirectBuffer(nullptr)
	{
		m_bufferOutputInfos =
		{
			{ "IndirectDraw", RenderPassBufferInfo(AccessFlags::Write, MaterialStageFlags::Transfer, MaterialAccessFlags::TransferWrite, nullptr) }
		};
	}

	void FrustumCullingPass::SetCullingMode(CullingMode mode)
	{
		if (m_mode == mode)
			return;

		m_mode = mode;
		int32_t modeInt = static_cast<int32_t>(mode);
		m_material->SetSpecialisationConstant("cullingMode", modeInt);
	}

	bool FrustumCullingPass::Build(const Renderer& renderer,
		const std::unordered_map<std::string, IRenderImage*>& imageInputs,
		const std::unordered_map<std::string, IRenderImage*>& imageOutputs,
		const std::unordered_map<std::string, IBuffer*>& bufferInputs,
		const std::unordered_map<std::string, IBuffer*>& bufferOutputs)
	{
		m_built = false;
		return true;
	}

	void FrustumCullingPass::ClearResources()
	{
		m_indirectBuffer.reset();
		auto& bufferInfo = m_bufferOutputInfos.at("IndirectDraw");
		bufferInfo.Buffer = nullptr;
		IComputePass::ClearResources();
	}

	bool FrustumCullingPass::FrustumPassBuild(const Renderer& renderer, IRenderImage* occlusionImage)
	{
		// If scene manager has not been built or is empty, mark the pass as done so drawing is skipped for this pass.
		if (!m_sceneGeometryBatch.IsBuilt() || m_sceneGeometryBatch.GetVertexBuffers().empty())
			return true;

		const IBuffer& boundsBuffer = m_sceneGeometryBatch.GetBoundsBuffer();
		const IBuffer& indirectDrawBuffer = m_sceneGeometryBatch.GetIndirectDrawBuffer();
		m_occlusionImage = occlusionImage;

		const Camera& camera = renderer.GetCameraReadOnly();

		uint32_t meshCount = m_sceneGeometryBatch.GetMeshCapacity();
		m_drawCullData.znear = camera.GetNearFar().x;
		m_drawCullData.zfar = camera.GetNearFar().y;
		m_drawCullData.pyramidWidth = static_cast<float>(m_occlusionImage->GetDimensions().x);
		m_drawCullData.pyramidHeight = static_cast<float>(m_occlusionImage->GetDimensions().y);

		m_dispatchSize = (meshCount / 64) + 1;

		const std::vector<std::unique_ptr<IBuffer>>& frameInfoBuffers = renderer.GetFrameInfoBuffers();

		if (!m_material->BindUniformBuffers(0, frameInfoBuffers) ||
			!m_material->BindStorageBuffer(1, boundsBuffer) ||
			!m_material->BindStorageBuffer(2, indirectDrawBuffer) ||
			!m_material->BindStorageBuffer(3, m_indirectBuffer) ||
			!m_material->BindCombinedImageSampler(4, renderer.GetReductionSampler(), occlusionImage->GetView(), ImageLayout::ShaderReadOnly))
			return false;

		m_built = true;
		return true;
	}

	bool FrustumCullingPass::BuildResources(const Renderer& renderer)
	{
		ClearResources();

		if (!CreateIndirectBuffer(renderer, m_sceneGeometryBatch.GetMeshCapacity()))
		{
			return false;
		}

		auto& bufferInfo = m_bufferOutputInfos.at("IndirectDraw");
		bufferInfo.Buffer = m_indirectBuffer.get();

		return true;
	}


	bool FrustumCullingPass::CreateIndirectBuffer(const Renderer& renderer, uint32_t meshCount)
	{
		const IDevice& device = renderer.GetDevice();
		const IResourceFactory& resourceFactory = renderer.GetResourceFactory();

		m_indirectBuffer = resourceFactory.CreateBuffer();

		if (!m_indirectBuffer->Initialise("indirectBuffer", device, sizeof(uint32_t) + meshCount * sizeof(IndexedIndirectCommand),
			BufferUsageFlags::IndirectBuffer | BufferUsageFlags::StorageBuffer | BufferUsageFlags::TransferDst, MemoryUsage::GPUOnly, AllocationCreateFlags::None, SharingMode::Exclusive))
		{
			Logger::Error("Failed to initialise indirect buffer.");
			return false;
		}

		return true;
	}

	void FrustumCullingPass::Dispatch(const Renderer& renderer, const ICommandBuffer& commandBuffer,
		uint32_t frameIndex)
	{
		if (!m_built || m_mode == CullingMode::Paused)
			return;

		const IDevice& device = renderer.GetDevice();

		bool firstDraw = m_occlusionImage->GetLayout() == ImageLayout::Undefined;

		// Occlusion image may be used for the first time in this pass, so transition it to a shader read layout.
		std::unique_ptr<IMemoryBarriers> memoryBarriers = std::move(renderer.GetResourceFactory().CreateMemoryBarriers());
		m_occlusionImage->AppendImageLayoutTransitionExt(commandBuffer,
			MaterialStageFlags::ComputeShader, ImageLayout::ShaderReadOnly,
			MaterialAccessFlags::ShaderRead, *memoryBarriers);
		commandBuffer.MemoryBarrier(*memoryBarriers);

		m_material->BindMaterial(commandBuffer, BindPoint::Compute, frameIndex);

		const Camera& camera = renderer.GetCameraReadOnly();
		const glm::mat4& projection = camera.GetProjection();
		m_drawCullData.frustum = camera.GetProjectionFrustum();
		m_drawCullData.P00 = projection[0][0];
		m_drawCullData.P11 = projection[1][1];
		m_drawCullData.enableOcclusion = firstDraw ? 0 : 1;

		commandBuffer.FillBuffer(*m_indirectBuffer, 0, sizeof(uint32_t), 0);

		commandBuffer.PushConstants(m_material, ShaderStageFlags::Compute, 0, sizeof(DrawCullData), reinterpret_cast<const uint32_t*>(&m_drawCullData));
		commandBuffer.Dispatch(m_dispatchSize, 1, 1);
	}
}