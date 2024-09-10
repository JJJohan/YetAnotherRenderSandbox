#include "ShadowCullingPass.hpp"
#include "../Resources/IBuffer.hpp"
#include "../RenderResources/ShadowMap.hpp"
#include "../IResourceFactory.hpp"
#include "../IDevice.hpp"
#include "../Resources/ICommandBuffer.hpp"
#include "../Resources/GeometryBatch.hpp"
#include "../Renderer.hpp"

namespace Engine::Rendering
{
	ShadowCullingPass::ShadowCullingPass(const GeometryBatch& sceneGeometryBatch, const ShadowMap& shadowMap)
		: IComputePass("ShadowCulling", "ShadowCulling")
		, m_sceneGeometryBatch(sceneGeometryBatch)
		, m_built(false)
		, m_dispatchSize(0)
		, m_mode(CullingMode::FrustumAndOcclusion)
		, m_shadowIndirectBuffer(nullptr)
	{
		m_bufferOutputInfos =
		{
			{ "ShadowIndirectDraw", RenderPassBufferInfo(AccessFlags::Write, MaterialStageFlags::Transfer, MaterialAccessFlags::TransferWrite, nullptr) }
		};
	}

	void ShadowCullingPass::SetCullingMode(CullingMode mode)
	{
		if (m_mode == mode)
			return;

		m_mode = mode;
		int32_t modeInt = static_cast<int32_t>(mode);
		m_material->SetSpecialisationConstant("cullingMode", modeInt);
	}

	bool ShadowCullingPass::Build(const Renderer& renderer,
		const std::unordered_map<std::string, IRenderImage*>& imageInputs,
		const std::unordered_map<std::string, IRenderImage*>& imageOutputs,
		const std::unordered_map<std::string, IBuffer*>& bufferInputs,
		const std::unordered_map<std::string, IBuffer*>& bufferOutputs)
	{
		m_built = false;

		// If scene manager has not been built or is empty, mark the pass as done so drawing is skipped for this pass.
		if (!m_sceneGeometryBatch.IsBuilt() || m_sceneGeometryBatch.GetVertexBuffers().empty())
			return true;

		const IBuffer& boundsBuffer = m_sceneGeometryBatch.GetBoundsBuffer();
		const IBuffer& indirectDrawBuffer = m_sceneGeometryBatch.GetIndirectDrawBuffer();

		uint32_t meshCount = m_sceneGeometryBatch.GetMeshCapacity();

		m_dispatchSize = (meshCount / 64) + 1;

		const std::vector<std::unique_ptr<IBuffer>>& frameInfoBuffers = renderer.GetFrameInfoBuffers();
		const std::vector<std::unique_ptr<IBuffer>>& lightBuffers = renderer.GetLightBuffers();

		if (!m_material->BindUniformBuffers(0, frameInfoBuffers) ||
			!m_material->BindUniformBuffers(1, lightBuffers) ||
			!m_material->BindStorageBuffer(2, boundsBuffer) ||
			!m_material->BindStorageBuffer(3, indirectDrawBuffer) ||
			!m_material->BindStorageBuffer(4, m_shadowIndirectBuffer))
			return false;

		m_built = true;
		return IRenderNode::Build(renderer, imageInputs, imageOutputs, bufferInputs, bufferOutputs);
	}

	void ShadowCullingPass::ClearResources()
	{
		m_shadowIndirectBuffer.reset();
		auto& bufferInfo = m_bufferOutputInfos.at("ShadowIndirectDraw");
		bufferInfo.Buffer = nullptr;
		IComputePass::ClearResources();
	}

	bool ShadowCullingPass::BuildResources(const Renderer& renderer)
	{
		ClearResources();

		if (!CreateIndirectBuffer(renderer, m_sceneGeometryBatch.GetMeshCapacity()))
		{
			return false;
		}

		auto& bufferInfo = m_bufferOutputInfos.at("ShadowIndirectDraw");
		bufferInfo.Buffer = m_shadowIndirectBuffer.get();

		return true;
	}

	bool ShadowCullingPass::CreateIndirectBuffer(const Renderer& renderer, uint32_t meshCount)
	{
		const IResourceFactory& resourceFactory = renderer.GetResourceFactory();
		const IDevice& device = renderer.GetDevice();

		uint32_t cascadeCount = renderer.GetShadowMap().GetCascadeCount();
		m_shadowIndirectBuffer = resourceFactory.CreateBuffer();

		// Create one single buffer with the capacity to hold each cascade.
		if (!m_shadowIndirectBuffer->Initialise("shadowIndirectBuffer", device, cascadeCount * (sizeof(uint32_t) + meshCount * sizeof(IndexedIndirectCommand)),
			BufferUsageFlags::IndirectBuffer | BufferUsageFlags::StorageBuffer | BufferUsageFlags::TransferDst, MemoryUsage::AutoPreferDevice, AllocationCreateFlags::None, SharingMode::Exclusive))
		{
			Logger::Error("Failed to initialise indirect buffer.");
			return false;
		}

		return true;
	}

	inline static glm::vec4 normalizePlane(const glm::vec4& p)
	{
		return p / glm::length(glm::vec3(p));
	}

	void ShadowCullingPass::Dispatch(const Renderer& renderer, const ICommandBuffer& commandBuffer,
		uint32_t frameIndex)
	{
		if (!m_built || m_mode == CullingMode::Paused)
			return;

		const Camera& camera = renderer.GetCameraReadOnly();
		const glm::vec4& frustum = camera.GetProjectionFrustum();

		const IDevice& device = renderer.GetDevice();

		m_material->BindMaterial(commandBuffer, BindPoint::Compute, frameIndex);

		commandBuffer.FillBuffer(*m_shadowIndirectBuffer, 0, sizeof(uint32_t) * 4, 0);

		commandBuffer.PushConstants(m_material, ShaderStageFlags::Compute, 0, sizeof(glm::vec4), reinterpret_cast<const uint32_t*>(&frustum));
		commandBuffer.Dispatch(m_dispatchSize, 1, 1);
	}
}