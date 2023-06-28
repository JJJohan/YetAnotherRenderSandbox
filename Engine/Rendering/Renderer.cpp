#include "Renderer.hpp"
#include "Core/SceneManager.hpp"
#include "Core/Logging/Logger.hpp"
#include "Vulkan/VulkanRenderer.hpp"
#include "OS/Window.hpp"
#include "Resources/IDevice.hpp"
#include "Resources/IPhysicalDevice.hpp"
#include "Resources/IResourceFactory.hpp"
#include "Resources/IBuffer.hpp"
#include "Resources/IMaterialManager.hpp"
#include "Resources/ISwapChain.hpp"
#include "Resources/FrameInfoUniformBuffer.hpp"
#include "Resources/LightUniformBuffer.hpp"
#include "Passes/IRenderPass.hpp"
#include "Passes/SceneOpaquePass.hpp"
#include "Passes/SceneShadowPass.hpp"
#include "Passes/CombinePass.hpp"
#include "ShadowMap.hpp"
#include "GBuffer.hpp"
#include "PostProcessing.hpp"

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
		, m_gBuffer(std::make_unique<GBuffer>())
		, m_shadowMap(std::make_unique<ShadowMap>())
		, m_postProcessing(nullptr)
		, m_renderGraph(std::make_unique<RenderGraph>())
		, m_materialManager(nullptr)
		, m_physicalDevice(nullptr)
		, m_device(nullptr)
		, m_swapChain(nullptr)
		, m_renderStats(nullptr)
		, m_uiManager(nullptr)
		, m_renderPasses()
		, m_sceneGeometryBatch()
		, m_sceneManager(std::make_unique<SceneManager>())
	{
		m_postProcessing = std::make_unique<PostProcessing>(*m_gBuffer);
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

		m_sceneGeometryBatch.reset();
		m_postProcessing.reset();
		m_shadowMap.reset();
		m_gBuffer.reset();
		m_uiManager.reset();
		m_sceneManager.reset();
		m_renderStats.reset();
		m_materialManager.reset();
		m_renderGraph.reset();
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
		frameInfo->jitter = m_postProcessing->IsEnabled() ? m_postProcessing->GetTAAJitter(size) : glm::vec2();
	}

	bool Renderer::Initialise()
	{
		m_lastWindowSize = m_window.GetSize();
		m_depthFormat = m_physicalDevice->FindDepthFormat();

		if (!m_materialManager->Initialise(*m_physicalDevice, *m_device, m_maxConcurrentFrames, m_swapChain->GetFormat(), m_depthFormat)
			|| !CreateFrameInfoUniformBuffer()
			|| !CreateLightUniformBuffer()
		    || !m_shadowMap->Rebuild(*m_device, *m_resourceFactory, m_depthFormat)
			|| !m_gBuffer->Initialise(*m_physicalDevice, *m_device, *m_materialManager, *m_resourceFactory,
				m_depthFormat, m_lastWindowSize, m_frameInfoBuffers, m_lightBuffers, *m_shadowMap)
			|| !m_postProcessing->Initialise(*m_physicalDevice, *m_device, *m_materialManager, *m_resourceFactory, m_lastWindowSize))
		{
			return false;
		}

		const uint32_t renderPassCount = m_shadowMap->GetCascadeCount() + 4; // Shadow cascades, scene, resolve, TAA & UI
		if (!m_renderStats->Initialise(*m_physicalDevice, *m_device, renderPassCount))
		{
			return false;
		}

		std::unique_ptr<SceneOpaquePass> sceneOpaquePass = std::make_unique<SceneOpaquePass>();
		m_renderPasses.emplace_back(std::move(sceneOpaquePass));

		std::unique_ptr<SceneShadowPass> sceneShadowPass = std::make_unique<SceneShadowPass>();
		m_renderPasses.emplace_back(std::move(sceneShadowPass));

		std::unique_ptr<CombinePass> combinePass = std::make_unique<CombinePass>();
		m_renderPasses.emplace_back(std::move(combinePass));

		for (const std::unique_ptr<IRenderPass>& pass : m_renderPasses)
		{
			if (!m_renderGraph->AddPass(pass.get()))
			{
				Logger::Error("Failed to add pass to render graph.");
				return false;
			}
		}

		std::vector<const IRenderPass*> postProcessPasses = m_postProcessing->GetRenderPasses();
		for (const IRenderPass* pass : postProcessPasses)
		{
			if (!m_renderGraph->AddPass(pass))
			{
				Logger::Error("Failed to add post process pass to render graph.");
				return false;
			}
		}

		if (!m_renderGraph->Build())
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