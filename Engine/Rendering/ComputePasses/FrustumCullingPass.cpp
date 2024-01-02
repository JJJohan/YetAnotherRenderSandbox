#include "FrustumCullingPass.hpp"
#include "../Resources/IBuffer.hpp"
#include "../IDevice.hpp"
#include "../Resources/ICommandBuffer.hpp"
#include "../Resources/GeometryBatch.hpp"
#include "../Renderer.hpp"

using namespace Engine::Logging;

namespace Engine::Rendering
{
	FrustumCullingPass::FrustumCullingPass(const GeometryBatch& sceneGeometryBatch)
		: IComputePass("FrustumCulling", "FrustumCulling")
		, m_sceneGeometryBatch(sceneGeometryBatch)
		, m_built(false)
		, m_dispatchSize(0)
	{
		m_bufferOutputs =
		{
			{"IndirectDraw", nullptr}
		};
	}	
	
	bool FrustumCullingPass::Build(const Renderer& renderer)
	{
		m_built = false;
		ClearResources();

		const IDevice& device = renderer.GetDevice();

		// If scene manager has not been built or is empty, mark the pass as done so drawing is skipped for this pass.
		if (!m_sceneGeometryBatch.IsBuilt() || m_sceneGeometryBatch.GetVertexBuffers().empty())
			return true;

		const IBuffer& boundsBuffer = m_sceneGeometryBatch.GetBoundsBuffer();
		const IBuffer& indirectDrawBuffer = m_sceneGeometryBatch.GetIndirectDrawBuffer();

		m_dispatchSize = (m_sceneGeometryBatch.GetMeshCapacity() / 64) + 1;

		const std::vector<std::unique_ptr<IBuffer>>& frameInfoBuffers = renderer.GetFrameInfoBuffers();

		if (!m_material->BindUniformBuffers(0, frameInfoBuffers) ||
			!m_material->BindStorageBuffer(1, boundsBuffer) ||
			!m_material->BindStorageBuffer(2, indirectDrawBuffer))
			return false;

		m_built = true;
		return true;
	}

	void FrustumCullingPass::Dispatch(const IDevice& device, const ICommandBuffer& commandBuffer,
		uint32_t frameIndex)
	{
		if (!m_built)
			return;

		m_material->BindMaterial(commandBuffer, BindPoint::Compute, frameIndex);
		commandBuffer.Dispatch(m_dispatchSize, 1, 1);
	}
}