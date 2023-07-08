#include "SceneShadowPass.hpp"
#include "../Resources/IBuffer.hpp"
#include "../Resources/IRenderImage.hpp"
#include "../IDevice.hpp"
#include "../Resources/ICommandBuffer.hpp"
#include "../Resources/GeometryBatch.hpp"
#include "../Renderer.hpp"

namespace Engine::Rendering
{
	SceneShadowPass::SceneShadowPass(const GeometryBatch& sceneGeometryBatch)
		: IRenderPass("SceneShadow", "Shadow")
		, m_sceneGeometryBatch(sceneGeometryBatch)
		, m_built(false)
	{
		m_imageInputs =
		{
			"Shadows"
		};

		m_imageOutputs = 
		{
			"Shadows"
		};
	}

	bool SceneShadowPass::Build(const Renderer& renderer, const std::unordered_map<const char*, IRenderImage*>& imageInputs,
		const std::unordered_map<const char*, IBuffer*>& bufferInputs)
	{
		m_built = false;

		const std::vector<std::unique_ptr<IBuffer>>& frameInfoBuffers = renderer.GetFrameInfoBuffers();
		const std::vector<std::unique_ptr<IBuffer>>& lightBuffers = renderer.GetLightBuffers();
		const IImageSampler& linearSampler = renderer.GetLinearSampler();

		m_imageResources["Shadows"] = imageInputs.at("Shadows");

		// If scene manager has not been built or is empty, mark the pass as done so drawing is skipped for this pass.
		if (!m_sceneGeometryBatch.IsBuilt() || m_sceneGeometryBatch.GetVertexBuffers().empty())
			return true;

		const IBuffer& meshInfoBuffer = m_sceneGeometryBatch.GetMeshInfoBuffer();
		const std::vector<std::unique_ptr<IRenderImage>>& imageArray = m_sceneGeometryBatch.GetImages();

		std::vector<const IImageView*> imageViews(imageArray.size());
		for (size_t i = 0; i < imageArray.size(); ++i)
			imageViews[i] = &imageArray[i]->GetView();

		if (!m_material->BindUniformBuffers(0, frameInfoBuffers) ||
			!m_material->BindUniformBuffers(1, lightBuffers) ||
			!m_material->BindStorageBuffer(2, meshInfoBuffer) ||
			!m_material->BindSampler(3, linearSampler) ||
			!m_material->BindImageViews(4, imageViews))
			return false;

		m_built = true;
		return true;
	}

	void SceneShadowPass::Draw(const IDevice& device, const ICommandBuffer& commandBuffer, uint32_t frameIndex) const
	{
		if (!m_built)
			return;

		uint32_t cascadeIndex = m_cascadeIndex;
		commandBuffer.PushConstants(m_material, ShaderStageFlags::Vertex, 0, sizeof(uint32_t), &cascadeIndex);

		if (cascadeIndex == 0)
		{
			const std::vector<std::unique_ptr<IBuffer>>& vertexBuffers = m_sceneGeometryBatch.GetVertexBuffers();
			std::vector<size_t> vertexBufferOffsets;
			vertexBufferOffsets.resize(2);
			std::vector<IBuffer*> vertexBufferViews = { vertexBuffers[0].get(), vertexBuffers[1].get() };

			m_material->BindMaterial(commandBuffer, frameIndex);
			commandBuffer.BindVertexBuffers(0, vertexBufferViews, vertexBufferOffsets);
			commandBuffer.BindIndexBuffer(m_sceneGeometryBatch.GetIndexBuffer(), 0, IndexType::Uint32);
		}

		uint32_t drawCount = m_sceneGeometryBatch.GetMeshCapacity(); // TODO: Compute counted, after culling, etc.
		commandBuffer.DrawIndexedIndirect(m_sceneGeometryBatch.GetIndirectDrawBuffer(), 0, drawCount, sizeof(IndexedIndirectCommand));
	}
}