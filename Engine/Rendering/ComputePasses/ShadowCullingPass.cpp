#include "ShadowCullingPass.hpp"
#include "../Resources/IBuffer.hpp"
#include "../RenderResources/ShadowMap.hpp"
#include "../IResourceFactory.hpp"
#include "../IDevice.hpp"
#include "../Resources/ICommandBuffer.hpp"
#include "../Resources/GeometryBatch.hpp"
#include "../Renderer.hpp"

using namespace Engine::Logging;

namespace Engine::Rendering
{
	ShadowCullingPass::ShadowCullingPass(const GeometryBatch& sceneGeometryBatch)
		: IComputePass("ShadowCulling", "ShadowCulling")
		, m_sceneGeometryBatch(sceneGeometryBatch)
		, m_built(false)
		, m_dispatchSize(0)
		, m_mode(CullingMode::FrustumAndOcclusion)
		, m_drawCullData()
		, m_shadowIndirectBuffer(nullptr)
	{
		m_bufferOutputs =
		{
			{ "ShadowIndirectDraw", nullptr }
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

		const Camera& camera = renderer.GetCameraReadOnly();

		uint32_t meshCount = m_sceneGeometryBatch.GetMeshCapacity();
		m_drawCullData.znear = camera.GetNearFar().x;
		m_drawCullData.zfar = camera.GetNearFar().y;

		m_dispatchSize = (meshCount / 64) + 1;

		const std::vector<std::unique_ptr<IBuffer>>& frameInfoBuffers = renderer.GetFrameInfoBuffers();

		if (!m_material->BindUniformBuffers(0, frameInfoBuffers) ||
			!m_material->BindStorageBuffer(1, boundsBuffer) ||
			!m_material->BindStorageBuffer(2, indirectDrawBuffer) ||
			!m_material->BindStorageBuffer(3, m_shadowIndirectBuffer))
			return false;

		m_built = true;
		return true;
	}

	void ShadowCullingPass::ClearResources()
	{
		m_shadowIndirectBuffer.reset();
		m_bufferOutputs["ShadowIndirectDraw"] = nullptr;
		IComputePass::ClearResources();
	}

	bool ShadowCullingPass::BuildResources(const Renderer& renderer)
	{
		ClearResources();

		if (!CreateIndirectBuffer(renderer, m_sceneGeometryBatch.GetMeshCapacity()))
		{
			return false;
		}

		m_bufferOutputs["ShadowIndirectDraw"] = m_shadowIndirectBuffer.get();
		return true;
	}

	bool ShadowCullingPass::CreateIndirectBuffer(const Renderer& renderer, uint32_t meshCount)
	{
		const IResourceFactory& resourceFactory = renderer.GetResourceFactory();
		const IDevice& device = renderer.GetDevice();

		uint32_t cascadeCount = renderer.GetShadowMap().GetCascadeCount();
		m_shadowIndirectBuffer = resourceFactory.CreateBuffer();

		// Create one single buffer with the capacity to hold each cascade.
		if (!m_shadowIndirectBuffer->Initialise("shadowIndirectBuffer", device, sizeof(uint32_t) + meshCount * cascadeCount * sizeof(IndexedIndirectCommand),
			BufferUsageFlags::IndirectBuffer | BufferUsageFlags::StorageBuffer | BufferUsageFlags::TransferDst, MemoryUsage::GPUOnly, AllocationCreateFlags::None, SharingMode::Exclusive))
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

		const IDevice& device = renderer.GetDevice();

		m_material->BindMaterial(commandBuffer, BindPoint::Compute, frameIndex);

		// TODO: Set up shadow frustum planes instead of simply copying the camera frustums.. Currently this is mostly just duplicating the frustum culling pass.

		const Camera& camera = renderer.GetCameraReadOnly();
		const glm::mat4& projection = camera.GetProjection();
		glm::mat4 projectionT = transpose(projection);
		glm::vec4 frustumX = normalizePlane(projectionT[3] + projectionT[0]); // x + w < 0
		glm::vec4 frustumY = normalizePlane(projectionT[3] + projectionT[1]); // y + w < 0

		m_drawCullData.P00 = projection[0][0];
		m_drawCullData.P11 = projection[1][1];
		m_drawCullData.frustum.x = frustumX.x;
		m_drawCullData.frustum.y = frustumX.z;
		m_drawCullData.frustum.z = frustumY.y;
		m_drawCullData.frustum.w = frustumY.z;

		commandBuffer.FillBuffer(*m_shadowIndirectBuffer, 0, sizeof(uint32_t), 0);
		commandBuffer.MemoryBarrier(MaterialStageFlags::Transfer, MaterialAccessFlags::TransferWrite,
			MaterialStageFlags::ComputeShader, MaterialAccessFlags::ShaderRead | MaterialAccessFlags::ShaderWrite);
		commandBuffer.PushConstants(m_material, ShaderStageFlags::Compute, 0, sizeof(DrawCullData), reinterpret_cast<uint32_t*>(&m_drawCullData));
		commandBuffer.Dispatch(m_dispatchSize, 1, 1);
		commandBuffer.MemoryBarrier(MaterialStageFlags::ComputeShader, MaterialAccessFlags::ShaderWrite,
			MaterialStageFlags::DrawIndirect, MaterialAccessFlags::IndirectCommandRead);
	}
}