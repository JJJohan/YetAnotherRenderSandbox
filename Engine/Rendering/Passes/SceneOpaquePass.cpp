#include "SceneOpaquePass.hpp"
#include "../Resources/IBuffer.hpp"
#include "../Resources/IRenderImage.hpp"
#include "../IDevice.hpp"
#include "../Resources/ICommandBuffer.hpp"
#include "../Resources/GeometryBatch.hpp"
#include "../Renderer.hpp"

namespace Engine::Rendering
{
	SceneOpaquePass::SceneOpaquePass(const GeometryBatch& sceneGeometryBatch)
		: IRenderPass("SceneOpaque", "PBR")
		, m_sceneGeometryBatch(sceneGeometryBatch)
		, m_built(false)
	{
		m_imageInputs =
		{
			{"Albedo", nullptr},
			{"WorldNormal", nullptr},
			{"WorldPos", nullptr},
			{"MetalRoughness", nullptr},
			{"Velocity", nullptr},
			{"Depth", nullptr}
		};

		m_imageOutputs =
		{
			{"Albedo", nullptr},
			{"WorldNormal", nullptr},
			{"WorldPos", nullptr},
			{"MetalRoughness", nullptr},
			{"Velocity", nullptr},
			{"Depth", nullptr}
		};
	}

	bool SceneOpaquePass::Build(const Renderer& renderer, const std::unordered_map<const char*, IRenderImage*>& imageInputs,
		const std::unordered_map<const char*, IBuffer*>& bufferInputs)
	{
		m_built = false;
		const std::vector<std::unique_ptr<IBuffer>>& frameInfoBuffers = renderer.GetFrameInfoBuffers();
		const std::vector<std::unique_ptr<IBuffer>>& lightBuffers = renderer.GetLightBuffers();
		const IImageSampler& linearSampler = renderer.GetLinearSampler();

		if (!IRenderPass::Build(renderer, imageInputs, bufferInputs))
			return false;

		m_colourAttachments.clear();
		m_colourAttachments.emplace_back(m_material->GetColourAttachmentInfo(0, m_imageInputs.at("Albedo")));
		m_colourAttachments.emplace_back(m_material->GetColourAttachmentInfo(1, m_imageInputs.at("WorldNormal")));
		m_colourAttachments.emplace_back(m_material->GetColourAttachmentInfo(2, m_imageInputs.at("WorldPos")));
		m_colourAttachments.emplace_back(m_material->GetColourAttachmentInfo(3, m_imageInputs.at("MetalRoughness")));
		m_colourAttachments.emplace_back(m_material->GetColourAttachmentInfo(4, m_imageInputs.at("Velocity")));

		m_depthAttachment = AttachmentInfo(m_imageInputs.at("Depth"), ImageLayout::DepthStencilAttachment, AttachmentLoadOp::Clear, AttachmentStoreOp::Store, ClearValue(1.0f));

		// If scene manager has not been built or is empty, mark the pass as done so drawing is skipped for this pass.
		if (!m_sceneGeometryBatch.IsBuilt() || m_sceneGeometryBatch.GetVertexBuffers().empty())
			return true;

		const IBuffer& meshInfoBuffer = m_sceneGeometryBatch.GetMeshInfoBuffer();
		const std::vector<std::unique_ptr<IRenderImage>>& imageArray = m_sceneGeometryBatch.GetImages();

		std::vector<const IImageView*> imageViews(imageArray.size());
		for (size_t i = 0; i < imageArray.size(); ++i)
			imageViews[i] = &imageArray[i]->GetView();

		if (!m_material->BindUniformBuffers(0, frameInfoBuffers) ||
			!m_material->BindStorageBuffer(1, meshInfoBuffer) ||
			!m_material->BindSampler(2, linearSampler) ||
			!m_material->BindImageViews(3, imageViews))
			return false;

		m_built = true;
		return true;
	}

	void SceneOpaquePass::Draw(const IDevice& device, const ICommandBuffer& commandBuffer,
		const glm::uvec2& size, uint32_t frameIndex, uint32_t layerIndex)
	{
		if (!m_built)
			return;

		m_material->BindMaterial(commandBuffer, frameIndex);

		const std::vector<std::unique_ptr<IBuffer>>& vertexBuffers = m_sceneGeometryBatch.GetVertexBuffers();
		uint32_t vertexBufferCount = static_cast<uint32_t>(vertexBuffers.size());

		std::vector<size_t> vertexBufferOffsets;
		vertexBufferOffsets.resize(vertexBufferCount);
		std::vector<IBuffer*> vertexBufferViews;
		vertexBufferViews.reserve(vertexBufferCount);
		for (const auto& buffer : vertexBuffers)
		{
			vertexBufferViews.emplace_back(buffer.get());
		}

		commandBuffer.BindVertexBuffers(0, vertexBufferViews, vertexBufferOffsets);
		commandBuffer.BindIndexBuffer(m_sceneGeometryBatch.GetIndexBuffer(), 0, IndexType::Uint32);

		uint32_t drawCount = m_sceneGeometryBatch.GetMeshCapacity(); // TODO: Compute counted, after culling, etc.
		commandBuffer.DrawIndexedIndirect(m_sceneGeometryBatch.GetIndirectDrawBuffer(), 0, drawCount, sizeof(IndexedIndirectCommand));
	}
}