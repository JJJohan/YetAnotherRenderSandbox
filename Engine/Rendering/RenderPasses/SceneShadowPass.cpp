#include "SceneShadowPass.hpp"
#include "../Resources/IBuffer.hpp"
#include "../Resources/IRenderImage.hpp"
#include "../IDevice.hpp"
#include "../Resources/ICommandBuffer.hpp"
#include "../Resources/GeometryBatch.hpp"
#include "../Renderer.hpp"
#include "../RenderResources/ShadowMap.hpp"

namespace Engine::Rendering
{
	SceneShadowPass::SceneShadowPass(const GeometryBatch& sceneGeometryBatch, const ShadowMap& shadowMap)
		: IRenderPass("SceneShadow", "Shadow")
		, m_sceneGeometryBatch(sceneGeometryBatch)
		, m_built(false)
		, m_shadowMap(shadowMap)
		, m_shadowResolution(4096)
		, m_indirectDrawBuffer(nullptr)
	{
		m_imageInputInfos =
		{
			{"Shadows", RenderPassImageInfo(Format::PlaceholderDepth, false, shadowMap.GetExtent())}
		};

		m_imageOutputInfos = 
		{
			{"Shadows", RenderPassImageInfo(Format::PlaceholderDepth, false, shadowMap.GetExtent())}
		};

		m_bufferInputs =
		{
			{"ShadowIndirectDraw", nullptr}
		};
	}

	void SceneShadowPass::UpdatePlaceholderFormats(Format swapchainFormat, Format depthFormat)
	{
		m_imageInputInfos.at("Shadows").Format = depthFormat;
		m_imageOutputInfos.at("Shadows").Format = depthFormat;
	}

	bool SceneShadowPass::Build(const Renderer& renderer,
		const std::unordered_map<std::string, IRenderImage*>& imageInputs,
		const std::unordered_map<std::string, IRenderImage*>& imageOutputs,
		const std::unordered_map<std::string, IBuffer*>& bufferInputs,
		const std::unordered_map<std::string, IBuffer*>& bufferOutputs)
	{
		m_built = false;

		ClearResources();

		const std::vector<std::unique_ptr<IBuffer>>& frameInfoBuffers = renderer.GetFrameInfoBuffers();
		const std::vector<std::unique_ptr<IBuffer>>& lightBuffers = renderer.GetLightBuffers();
		const IImageSampler& shadowSampler = renderer.GetShadowSampler();
		m_layerCount = renderer.GetShadowMap().GetCascadeCount();
		IRenderImage* shadowImage = imageInputs.at("Shadows");
		m_shadowResolution = shadowImage->GetDimensions();
		m_depthAttachment = AttachmentInfo(shadowImage, ImageLayout::DepthStencilAttachment, AttachmentLoadOp::Clear, AttachmentStoreOp::Store, ClearValue(1.0f));

		m_indirectDrawBuffer = bufferInputs.at("ShadowIndirectDraw");

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
			!m_material->BindSampler(3, shadowSampler) ||
			!m_material->BindImageViews(4, imageViews))
			return false;

		m_built = true;
		return true;
	}

	void SceneShadowPass::Draw(const IDevice& device, const ICommandBuffer& commandBuffer,
		const glm::uvec2& size, uint32_t frameIndex, uint32_t layerIndex)
	{
		if (!m_built)
			return;

		commandBuffer.PushConstants(m_material, ShaderStageFlags::Vertex, 0, sizeof(uint32_t), &layerIndex);

		if (layerIndex == 0)
		{
			const std::vector<std::unique_ptr<IBuffer>>& vertexBuffers = m_sceneGeometryBatch.GetVertexBuffers();
			std::vector<size_t> vertexBufferOffsets;
			vertexBufferOffsets.resize(2);
			std::vector<IBuffer*> vertexBufferViews = { vertexBuffers[0].get(), vertexBuffers[1].get() };

			m_depthAttachment->loadOp = AttachmentLoadOp::Load;
			m_material->BindMaterial(commandBuffer, BindPoint::Graphics, frameIndex);
			commandBuffer.BindVertexBuffers(0, vertexBufferViews, vertexBufferOffsets);
			commandBuffer.BindIndexBuffer(m_sceneGeometryBatch.GetIndexBuffer(), 0, IndexType::Uint32);
		}
		else if (layerIndex == m_layerCount - 1)
		{
			// Reset to 'Clear' for next frame.
			m_depthAttachment->loadOp = AttachmentLoadOp::Clear;
		}

		uint32_t maxDrawCount = m_sceneGeometryBatch.GetMeshCapacity();
		commandBuffer.DrawIndexedIndirectCount(*m_indirectDrawBuffer, sizeof(uint32_t), *m_indirectDrawBuffer, 0, maxDrawCount, sizeof(IndexedIndirectCommand));
	}
}