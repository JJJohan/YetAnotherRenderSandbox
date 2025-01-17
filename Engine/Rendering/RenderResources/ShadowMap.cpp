#include "ShadowMap.hpp"
#include "Core/Logger.hpp"
#include "../Camera.hpp"
#include "../IDevice.hpp"
#include "../Resources/IRenderImage.hpp"
#include "../IResourceFactory.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include "../Renderer.hpp"

namespace Engine::Rendering
{
	constexpr int DefaultCascadeCount = 4;

	ShadowMap::ShadowMap()
		: IRenderResource("ShadowMap")
		, m_shadowImage(nullptr)
		, m_cascadeMatrices(DefaultCascadeCount)
		, m_cascadeSplits(DefaultCascadeCount)
		, m_cascadeCount(DefaultCascadeCount)
		, m_extent(4096, 4096, 1)
	{
		m_imageOutputInfos =
		{
			{"Shadows", {AccessFlags::Write} }
		};
	}

	ShadowCascadeData ShadowMap::UpdateCascades(const Camera& camera, const glm::vec3& lightDir)
	{
		const float cascadeSplitLambda = 0.95f;

		std::vector<float> cascadeSplits(m_cascadeCount);

		const glm::vec2& nearFar = camera.GetNearFar();
		float clipRange = nearFar.y - nearFar.x;

		float minZ = nearFar.x;
		float maxZ = nearFar.x + clipRange;

		float range = maxZ - minZ;
		float ratio = maxZ / minZ;

		// Calculate split depths based on view camera frustum
		// Based on method presented in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
		for (uint32_t i = 0; i < m_cascadeCount; i++)
		{
			float p = (i + 1) / static_cast<float>(m_cascadeCount);
			float log = minZ * std::pow(ratio, p);
			float uniform = minZ + range * p;
			float d = cascadeSplitLambda * (log - uniform) + uniform;
			cascadeSplits[i] = (d - nearFar.x) / clipRange;
		}

		// Calculate orthographic projection matrix for each cascade
		float lastSplitDist = 0.0;
		glm::mat4 invCam = glm::inverse(camera.GetViewProjection());
		for (uint32_t i = 0; i < m_cascadeCount; i++)
		{
			float splitDist = cascadeSplits[i];

			glm::vec3 frustumCorners[8] = {
				glm::vec3(-1.0f,  1.0f, 0.0f),
				glm::vec3(1.0f,  1.0f, 0.0f),
				glm::vec3(1.0f, -1.0f, 0.0f),
				glm::vec3(-1.0f, -1.0f, 0.0f),
				glm::vec3(-1.0f,  1.0f,  1.0f),
				glm::vec3(1.0f,  1.0f,  1.0f),
				glm::vec3(1.0f, -1.0f,  1.0f),
				glm::vec3(-1.0f, -1.0f,  1.0f),
			};

			// Project frustum corners into world space
			for (uint32_t i = 0; i < 8; i++)
			{
				glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[i], 1.0f);
				frustumCorners[i] = invCorner / invCorner.w;
			}

			for (uint32_t i = 0; i < 4; i++)
			{
				glm::vec3 dist = frustumCorners[i + 4] - frustumCorners[i];
				frustumCorners[i + 4] = frustumCorners[i] + (dist * splitDist);
				frustumCorners[i] = frustumCorners[i] + (dist * lastSplitDist);
			}

			// Get frustum center
			glm::vec3 frustumCenter = glm::vec3(0.0f);
			for (uint32_t i = 0; i < 8; i++)
			{
				frustumCenter += frustumCorners[i];
			}
			frustumCenter /= 8.0f;

			float radius = 0.0f;
			for (uint32_t i = 0; i < 8; i++)
			{
				float distance = glm::length(frustumCorners[i] - frustumCenter);
				radius = glm::max(radius, distance);
			}
			radius = std::ceil(radius * 16.0f) / 16.0f;

			glm::vec3 maxExtents = glm::vec3(radius);
			glm::vec3 minExtents = -maxExtents;

			glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - lightDir * -minExtents.z, frustumCenter, glm::vec3(0.0f, 1.0f, 0.0f));
			glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, maxExtents.z - minExtents.z);

			// Store split distance and matrix in cascade
			m_cascadeSplits[i] = (nearFar.x + splitDist * clipRange) * -1.0f;
			m_cascadeMatrices[i] = lightOrthoMatrix * lightViewMatrix;

			lastSplitDist = cascadeSplits[i];
		}

		return GetShadowCascadeData();
	}


	void ShadowMap::SetResolution(uint32_t resolution)
	{
		m_extent = glm::vec3(resolution, resolution, 1.0f);
	}

	bool ShadowMap::CreateShadowImages(const IDevice& device, const IResourceFactory& resourceFactory, Format depthFormat)
	{
		m_shadowImage = std::move(resourceFactory.CreateRenderImage());
		if (!m_shadowImage->Initialise("ShadowImage", device, ImageType::e2D, depthFormat, m_extent, 1, m_cascadeCount, ImageTiling::Optimal,
			ImageUsageFlags::Sampled | ImageUsageFlags::DepthStencilAttachment | ImageUsageFlags::TransferDst,
			ImageAspectFlags::Depth, MemoryUsage::AutoPreferDevice, AllocationCreateFlags::None, SharingMode::Exclusive))
		{
			Logger::Error("Failed to create shadow image.");
			return false;
		}

		return true;
	}

	void ShadowMap::ClearResources()
	{
		m_shadowImage.reset();
	}

	bool ShadowMap::BuildResources(const Renderer& renderer)
	{
		ClearResources();

		const IDevice& device = renderer.GetDevice();
		const IResourceFactory& resourceFactory = renderer.GetResourceFactory();
		Format depthFormat = renderer.GetDepthFormat();

		if (!CreateShadowImages(device, resourceFactory, depthFormat))
		{
			return false;
		}

		m_imageOutputInfos["Shadows"] = RenderPassImageInfo(AccessFlags::Write, depthFormat, m_extent,
			ImageLayout::Undefined, MaterialStageFlags::None, MaterialAccessFlags::None, m_shadowImage.get());

		return true;
	}
}