#include "Renderer.hpp"
#include "Core/SceneManager.hpp"
#include "Core/Logging/Logger.hpp"
#include "Vulkan/VulkanRenderer.hpp"
#include "OS/Window.hpp"
#include "IDevice.hpp"
#include "IPhysicalDevice.hpp"
#include "IResourceFactory.hpp"
#include "Resources/IImageSampler.hpp"
#include "Resources/IBuffer.hpp"
#include "IMaterialManager.hpp"
#include "ISwapChain.hpp"
#include "Resources/FrameInfoUniformBuffer.hpp"
#include "Resources/LightUniformBuffer.hpp"
#include "Resources/GeometryBatch.hpp"
#include "Resources/ISemaphore.hpp"
#include "Resources/ICommandPool.hpp"
#include "Resources/ICommandBuffer.hpp"
#include "RenderResources/ShadowMap.hpp"
#include "PostProcessing.hpp"
#include "Passes/IRenderPass.hpp"

#include "Passes/SceneOpaquePass.hpp"
#include "Passes/SceneShadowPass.hpp"
#include "Passes/CombinePass.hpp"
#include "Passes/UIPass.hpp"

using namespace Engine::OS;
using namespace Engine::UI;
using namespace Engine::Logging;
using namespace Engine::Rendering::Vulkan;

namespace Engine::Rendering
{
	Renderer::Renderer(const Window& window, bool debug)
		: m_debug(debug)
		, m_window(window)
		, m_clearColour()
		, m_camera()
		, m_maxMultiSampleCount(1)
		, m_sunDirection(glm::normalize(glm::vec3(0.2f, -1.0f, 2.0f)))
		, m_sunColour(Colour(1.0f, 1.0f, 1.0f))
		, m_sunIntensity(1.0f)
		, m_debugMode(0)
		, m_resourceFactory(nullptr)
		, m_currentFrame(0)
		, m_lastWindowSize()
		, m_maxConcurrentFrames()
		, m_renderSettings()
		, m_depthFormat(Format::Undefined)
		, m_frameInfoBuffers()
		, m_lightBuffers()
		, m_frameInfoBufferData()
		, m_lightBufferData()
		, m_shadowMap(std::make_unique<ShadowMap>())
		, m_postProcessing(std::make_unique<PostProcessing>())
		, m_renderGraph(nullptr)
		, m_materialManager(nullptr)
		, m_physicalDevice(nullptr)
		, m_device(nullptr)
		, m_swapChain(nullptr)
		, m_renderStats(nullptr)
		, m_uiManager(nullptr)
		, m_renderPasses()
		, m_sceneManager(std::make_unique<SceneManager>())
		, m_linearSampler(nullptr)
		, m_nearestSampler(nullptr)
		, m_shadowSampler(nullptr)
		, m_sceneGeometryBatch(std::make_unique<GeometryBatch>(*this))
	{
	}

	Renderer::~Renderer()
	{
	}

	void Renderer::DestroyResources()
	{
		m_frameInfoBuffers.clear();
		m_lightBuffers.clear();
		m_frameInfoBufferData.clear();
		m_lightBufferData.clear();
		m_renderPasses.clear();

		m_renderGraph.reset();
		m_linearSampler.reset();
		m_nearestSampler.reset();
		m_shadowSampler.reset();
		m_sceneGeometryBatch.reset();
		m_postProcessing.reset();
		m_shadowMap.reset();
		m_uiManager.reset();
		m_sceneManager.reset();
		m_renderStats.reset();
		m_materialManager.reset();
	}


	bool Renderer::CreateFrameInfoUniformBuffer()
	{
		size_t bufferSize = sizeof(FrameInfoUniformBuffer);

		m_frameInfoBuffers.resize(m_maxConcurrentFrames);
		m_frameInfoBufferData.resize(m_maxConcurrentFrames);

		for (uint32_t i = 0; i < m_maxConcurrentFrames; ++i)
		{
			m_frameInfoBuffers[i] = std::move(m_resourceFactory->CreateBuffer());
			IBuffer& buffer = *m_frameInfoBuffers[i];

			if (!buffer.Initialise(bufferSize, BufferUsageFlags::UniformBuffer, MemoryUsage::Auto,
				AllocationCreateFlags::HostAccessSequentialWrite | AllocationCreateFlags::Mapped,
				SharingMode::Exclusive))
			{
				return false;
			}

			if (!buffer.GetMappedMemory(&m_frameInfoBufferData[i]))
			{
				return false;
			}
		}

		return true;
	}

	void Renderer::SetTemporalAAState(bool enabled)
	{
		m_renderSettings.m_temporalAA = enabled;
		m_renderGraph->SetPassEnabled("TAA", enabled);
	}

	bool Renderer::CreateLightUniformBuffer()
	{
		size_t bufferSize = sizeof(LightUniformBuffer);

		m_lightBuffers.resize(m_maxConcurrentFrames);
		m_lightBufferData.resize(m_maxConcurrentFrames);

		for (uint32_t i = 0; i < m_maxConcurrentFrames; ++i)
		{
			m_lightBuffers[i] = std::move(m_resourceFactory->CreateBuffer());
			IBuffer& buffer = *m_lightBuffers[i];

			if (!buffer.Initialise(bufferSize, BufferUsageFlags::UniformBuffer, MemoryUsage::Auto,
				AllocationCreateFlags::HostAccessSequentialWrite | AllocationCreateFlags::Mapped,
				SharingMode::Exclusive))
			{
				return false;
			}

			if (!buffer.GetMappedMemory(&m_lightBufferData[i]))
			{
				return false;
			}
		}

		return true;
	}

	void Renderer::UpdateFrameInfo()
	{
		const glm::vec2& size = m_window.GetSize();

		FrameInfoUniformBuffer* frameInfo = m_frameInfoBufferData[m_currentFrame];
		frameInfo->view = m_camera.GetView();
		frameInfo->viewPos = glm::vec4(m_camera.GetPosition(), 1.0f);
		frameInfo->prevViewProj = frameInfo->viewProj;
		frameInfo->viewSize = size;
		frameInfo->viewProj = m_camera.GetViewProjection();
		frameInfo->jitter = m_renderSettings.m_temporalAA ? m_postProcessing->GetTAAJitter() : glm::vec2();
	}

	bool Renderer::Initialise()
	{
		m_lastWindowSize = m_window.GetSize();
		m_depthFormat = m_physicalDevice->FindDepthFormat();

		m_renderGraph = std::make_unique<RenderGraph>(*m_renderStats);

		if (!m_materialManager->Initialise(*m_physicalDevice, *m_device, m_maxConcurrentFrames, m_swapChain->GetFormat(), m_depthFormat)
			|| !CreateFrameInfoUniformBuffer()
			|| !CreateLightUniformBuffer()
			|| !m_postProcessing->Initialise())
		{
			return false;
		}

		if (!m_renderGraph->AddResource(m_shadowMap.get()))
		{
			Logger::Error("Failed to add Shadow Map to render graph.");
			return false;
		}

		float maxAnisotropy = m_physicalDevice->GetMaxAnisotropy();

		m_linearSampler = std::move(m_resourceFactory->CreateImageSampler());
		if (!m_linearSampler->Initialise(*m_device, Filter::Linear, Filter::Linear, SamplerMipmapMode::Linear,
			SamplerAddressMode::Repeat, maxAnisotropy))
		{
			return false;
		}

		m_nearestSampler = std::move(m_resourceFactory->CreateImageSampler());
		if (!m_nearestSampler->Initialise(*m_device, Filter::Nearest, Filter::Nearest, SamplerMipmapMode::Nearest,
			SamplerAddressMode::Repeat, 1))
		{
			return false;
		}
		m_shadowSampler = std::move(m_resourceFactory->CreateImageSampler());
		if (!m_shadowSampler->Initialise(*m_device, Filter::Linear, Filter::Linear, SamplerMipmapMode::Linear,
			SamplerAddressMode::ClampToBorder, 1))
		{
			return false;
		}

		if (!m_renderGraph->Initialise(*m_physicalDevice, *m_device, *m_resourceFactory, m_maxConcurrentFrames))
		{
			Logger::Error("Failed to initialise render graph.");
			return false;
		}

		m_renderPasses["SceneOpaque"] = std::make_unique<SceneOpaquePass>(*m_sceneGeometryBatch);
		m_renderPasses["SceneShadow"] = std::make_unique<SceneShadowPass>(*m_sceneGeometryBatch, *m_shadowMap);
		m_renderPasses["Combine"] = std::make_unique<CombinePass>(*m_shadowMap);

		for (const auto& pair : m_renderPasses)
		{
			if (!m_renderGraph->AddPass(pair.second.get(), *m_materialManager))
			{
				Logger::Error("Failed to add pass '{}' to render graph.", pair.first);
				return false;
			}
		}

		std::vector<IRenderPass*> postProcessPasses = m_postProcessing->GetRenderPasses();
		for (IRenderPass* pass : postProcessPasses)
		{
			if (!m_renderGraph->AddPass(pass, *m_materialManager))
			{
				Logger::Error("Failed to add post process pass to render graph.");
				return false;
			}
		}

		// Add UI pass after post processing.
		m_renderPasses["UI"] = std::make_unique<UIPass>(*m_uiManager);
		if (!m_renderGraph->AddPass(m_renderPasses["UI"].get(), *m_materialManager))
		{
			Logger::Error("Failed to add pass 'UI' to render graph.");
			return false;
		}

		if (!m_renderGraph->Build(*this))
		{
			Logger::Error("Failed to build render graph.");
			return false;
		}

		return true;
	}

	std::unique_ptr<Renderer> Renderer::Create(RendererType rendererType, const Window& window, bool debug)
	{
		switch (rendererType)
		{
		case RendererType::VULKAN:
			return std::make_unique<VulkanRenderer>(window, debug);
		default:
			Logger::Error("Requested renderer type not supported.");
			return nullptr;
		}
	}

	void Renderer::SetDebugMode(uint32_t mode)
	{
		m_debugMode = mode;
		const std::unique_ptr<CombinePass>& combinePass = reinterpret_cast<const std::unique_ptr<CombinePass>&>(m_renderPasses.at("Combine"));
		combinePass->SetDebugMode(mode);
	}

	bool Renderer::Render()
	{
		const glm::uvec2& windowSize = m_swapChain->GetExtent();

		UpdateFrameInfo();

		bool drawUi = m_uiManager->GetDrawCallbackCount() > 0;
		std::unique_ptr<IRenderPass>& uiPass = m_renderPasses["UI"];
		if (uiPass->GetEnabled() != drawUi)
		{
			uiPass->SetEnabled(drawUi);
			m_renderGraph->MarkDirty();
		}

		// Update light buffer
		LightUniformBuffer* lightInfo = m_lightBufferData[m_currentFrame];
		lightInfo->sunLightColour = m_sunColour.GetVec4();
		lightInfo->sunLightIntensity = m_sunIntensity;
		lightInfo->sunLightDir = m_sunDirection;

		const uint32_t cascadeCount = m_shadowMap->GetCascadeCount();
		ShadowCascadeData cascadeData = m_shadowMap->UpdateCascades(m_camera, m_sunDirection);
		for (uint32_t i = 0; i < cascadeCount; ++i)
		{
			lightInfo->cascadeMatrices[i] = cascadeData.CascadeMatrices[i];
			lightInfo->cascadeSplits[i] = cascadeData.CascadeSplits[i];
		}

		m_camera.Update(windowSize);

		if (!m_renderGraph->Draw(*this, m_currentFrame))
		{
			return false;
		}

		return true;
	}

	void Renderer::SetMultiSampleCount(uint32_t multiSampleCount)
	{
		if (multiSampleCount > m_maxMultiSampleCount)
		{
			Logger::Error("Sample count of {} exceeds maximum supported value of {}.", multiSampleCount, m_maxMultiSampleCount);
			return;
		}

		m_renderSettings.m_multiSampleCount = std::clamp(multiSampleCount, 1U, m_maxMultiSampleCount);
	}
}