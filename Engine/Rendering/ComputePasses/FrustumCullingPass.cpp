#include "FrustumCullingPass.hpp"
#include "DepthReductionPass.hpp"
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
	FrustumCullingPass::FrustumCullingPass(const GeometryBatch& sceneGeometryBatch)
		: IComputePass("FrustumCulling", "FrustumCulling")
		, m_sceneGeometryBatch(sceneGeometryBatch)
		, m_built(false)
		, m_dispatchSize(0)
		, m_mode(CullingMode::FrustumAndOcclusion)
		, m_drawCullData()
		, m_occlusionImage(nullptr)
		, m_indirectBuffer(nullptr)
		, m_shadowIndirectBuffer(nullptr)
	{
		m_bufferOutputs =
		{
			{ "IndirectDraw", nullptr },
			{ "ShadowIndirectDraw", nullptr }
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
		ClearResources();
		m_indirectBuffer.reset();
		m_shadowIndirectBuffer.reset();
		m_bufferOutputs["IndirectDraw"] = nullptr;
		m_bufferOutputs["ShadowIndirectDraw"] = nullptr;
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
			!m_material->BindStorageBuffer(4, m_shadowIndirectBuffer) ||
			!m_material->BindCombinedImageSampler(5, renderer.GetReductionSampler(), occlusionImage->GetView(), ImageLayout::ShaderReadOnly))
			return false;

		m_built = true;
		return true;
	}

	bool FrustumCullingPass::BuildResources(const Renderer& renderer)
	{
		if (!BuildBuffers(renderer, m_sceneGeometryBatch.GetMeshCapacity()))
		{
			return false;
		}

		m_bufferOutputs["IndirectDraw"] = m_indirectBuffer.get();
		m_bufferOutputs["ShadowIndirectDraw"] = m_shadowIndirectBuffer.get();
		return true;
	}

	bool FrustumCullingPass::CreateIndirectBuffer(const IResourceFactory& resourceFactory, uint32_t meshCount, std::unique_ptr<IBuffer>& outBuffer)
	{
		outBuffer = resourceFactory.CreateBuffer();

		if (!outBuffer->Initialise(sizeof(uint32_t) + meshCount * sizeof(IndexedIndirectCommand), BufferUsageFlags::IndirectBuffer | BufferUsageFlags::StorageBuffer,
			MemoryUsage::GPUOnly, AllocationCreateFlags::None, SharingMode::Exclusive))
		{
			Logger::Error("Failed to initialise indirect buffer.");
			return false;
		}

		return true;
	}

	bool FrustumCullingPass::BuildBuffers(const Renderer& renderer, uint32_t meshCount)
	{
		const IResourceFactory& resourceFactory = renderer.GetResourceFactory();

		if (!CreateIndirectBuffer(resourceFactory, meshCount, m_indirectBuffer))
		{
			return false;
		}

		// Create one single buffer with the capacity to hold each cascade.
		uint32_t cascadeCount = renderer.GetShadowMap().GetCascadeCount();
		if (!CreateIndirectBuffer(resourceFactory, meshCount * cascadeCount, m_shadowIndirectBuffer))
		{
			return false;
		}

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

		bool firstDraw = m_occlusionImage->GetLayout() == ImageLayout::Undefined;

		// Occlusion image may be used for the first time in this pass, so transition it to a shader read layout.
		m_occlusionImage->TransitionImageLayoutExt(device, commandBuffer,
			firstDraw ? MaterialStageFlags::None : MaterialStageFlags::ComputeShader,
			m_occlusionImage->GetLayout(),
			firstDraw ? MaterialAccessFlags::None : MaterialAccessFlags::ShaderRead,
			MaterialStageFlags::ComputeShader, ImageLayout::ShaderReadOnly,
			MaterialAccessFlags::ShaderRead);

		m_material->BindMaterial(commandBuffer, BindPoint::Compute, frameIndex);

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
		m_drawCullData.enableOcclusion = firstDraw ? 0 : 1;

		// First we dispatch a single thread to reset the atomic counters used for the visible indirect draw command count.
		// If more counters begin to be used, a uniform buffer with counters reset once at the start of the frame might be a better solution.
		m_drawCullData.resetCounter = 1;
		commandBuffer.PushConstants(m_material, ShaderStageFlags::Compute, 0, sizeof(DrawCullData), reinterpret_cast<uint32_t*>(&m_drawCullData));
		commandBuffer.Dispatch(1, 1, 1);

		commandBuffer.MemoryBarrier(MaterialStageFlags::ComputeShader, MaterialAccessFlags::ShaderWrite, MaterialStageFlags::ComputeShader, MaterialAccessFlags::ShaderRead);

		m_drawCullData.resetCounter = 0;
		commandBuffer.PushConstants(m_material, ShaderStageFlags::Compute, 0, sizeof(DrawCullData), reinterpret_cast<uint32_t*>(&m_drawCullData));
		commandBuffer.Dispatch(m_dispatchSize, 1, 1);
	}
}