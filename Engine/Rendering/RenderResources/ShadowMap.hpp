#pragma once

#include "../Types.hpp"
#include <vector>
#include <memory>
#include "../Resources/AttachmentInfo.hpp"
#include "../Resources/IRenderImage.hpp"
#include "IRenderResource.hpp"

namespace Engine::Rendering
{
	class IDevice;
	class IResourceFactory;
	class Camera;

	struct ShadowCascadeData
	{
		const std::vector<float>& CascadeSplits;
		const std::vector<glm::mat4>& CascadeMatrices;

		ShadowCascadeData(const std::vector<float>& splits, const std::vector<glm::mat4>& matrices)
			: CascadeSplits(splits)
			, CascadeMatrices(matrices)
		{
		}
	};

	class ShadowMap : public IRenderResource
	{
	public:
		ShadowMap();

		virtual bool Build(const Renderer& renderer) override;

		ShadowCascadeData UpdateCascades(const Camera& camera, const glm::vec3& lightDir);

		inline ShadowCascadeData GetShadowCascadeData() const { return ShadowCascadeData(m_cascadeSplits, m_cascadeMatrices); }
		inline uint32_t GetCascadeCount() const { return m_cascadeCount; }
		inline const glm::uvec3& GetExtent() const { return m_extent; }

		inline virtual size_t GetMemoryUsage() const override
		{
			const size_t bytesPerTexel = 4;
			return bytesPerTexel * m_cascadeCount * m_extent.x * m_extent.y;
		}

	private:
		bool CreateShadowImages(const IDevice& device, const IResourceFactory& resourceFactory, Format depthFormat);

		glm::uvec3 m_extent;
		uint32_t m_cascadeCount;
		std::vector<float> m_cascadeSplits;
		std::vector<glm::mat4> m_cascadeMatrices;
		std::unique_ptr<IRenderImage> m_shadowImage;
	};
}