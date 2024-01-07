#include "FrustumCullingPass.hpp"
#include "DepthReductionPass.hpp"
#include "../Resources/IBuffer.hpp"
#include "../IResourceFactory.hpp"
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
		, m_mode(CullingMode::FrustumAndOcclusion)
		, m_drawCullData()
		, m_occlusionImage(nullptr)
	{
		m_bufferOutputs =
		{
			{"IndirectDraw", nullptr}
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
		const std::unordered_map<const char*, IRenderImage*>& imageInputs,
		const std::unordered_map<const char*, IRenderImage*>& imageOutputs)
	{
		m_built = false;
		ClearResources();

		return true;
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

		m_drawCullData.meshCount = m_sceneGeometryBatch.GetMeshCapacity();
		m_drawCullData.znear = camera.GetNearFar().x;
		m_drawCullData.zfar = camera.GetNearFar().y;
		m_drawCullData.pyramidWidth = static_cast<float>(m_occlusionImage->GetDimensions().x);
		m_drawCullData.pyramidHeight = static_cast<float>(m_occlusionImage->GetDimensions().y);

		m_dispatchSize = (m_drawCullData.meshCount / 64) + 1;

		const std::vector<std::unique_ptr<IBuffer>>& frameInfoBuffers = renderer.GetFrameInfoBuffers();

		if (!m_material->BindUniformBuffers(0, frameInfoBuffers) ||
			!m_material->BindStorageBuffer(1, boundsBuffer) ||
			!m_material->BindStorageBuffer(2, indirectDrawBuffer) ||
			!m_material->BindCombinedImageSampler(3, renderer.GetReductionSampler(), occlusionImage->GetView(), ImageLayout::ShaderReadOnly))
			return false;

		m_built = true;
		return true;
	}

	inline static glm::vec4 normalizePlane(const glm::vec4& p)
	{
		return p / glm::length(glm::vec3(p));
	}

	void FrustumCullingPass::Dispatch(const Renderer& renderer, const ICommandBuffer& commandBuffer,
		uint32_t frameIndex)
	{
		if (!m_built)
			return;

		const IDevice& device = renderer.GetDevice();

		m_material->BindMaterial(commandBuffer, BindPoint::Compute, frameIndex);

		m_occlusionImage->TransitionImageLayoutExt(device, commandBuffer,
			MaterialStageFlags::ComputeShader, ImageLayout::ShaderReadOnly, MaterialAccessFlags::ShaderRead);

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
		commandBuffer.PushConstants(m_material, ShaderStageFlags::Compute, 0, sizeof(DrawCullData), reinterpret_cast<uint32_t*>(&m_drawCullData));

		commandBuffer.Dispatch(m_dispatchSize, 1, 1);

		// TODO: Memory barrier for indirect buffer.
	}
}