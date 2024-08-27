#include "Renderer.hpp"
#include "Core/SceneManager.hpp"
#include "Core/Logger.hpp"
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

#include "RenderPasses/IRenderPass.hpp"
#include "RenderPasses/SceneOpaquePass.hpp"
#include "RenderPasses/SceneShadowPass.hpp"
#include "RenderPasses/CombinePass.hpp"
#include "RenderPasses/UIPass.hpp"

#include "ComputePasses/IComputePass.hpp"
#include "ComputePasses/FrustumCullingPass.hpp"
#include "ComputePasses/ShadowCullingPass.hpp"
#include "ComputePasses/DepthReductionPass.hpp"

using namespace Engine::OS;
using namespace Engine::UI;
using namespace Engine::Rendering::Vulkan;

namespace Engine::Rendering
{
	Renderer::Renderer(Window& window, bool debug)
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
		, m_computePasses()
		, m_sceneManager(std::make_unique<SceneManager>())
		, m_linearSampler(nullptr)
		, m_nearestSampler(nullptr)
		, m_shadowSampler(nullptr)
		, m_reductionSampler(nullptr)
		, m_sceneGeometryBatch(std::make_unique<GeometryBatch>(*this))
		, m_nvidiaReflex(nullptr)
		, m_asyncComputeEnabled(false)
		, m_asyncComputeSupported(false)
		, m_asyncComputePendingState(false)
	{
	}

	Renderer::~Renderer()
	{
	}

	void Renderer::DestroyResources()
	{
		m_window.UnregisterPrePollCallback(std::bind(&Renderer::OnWindowPrePoll, this));
		m_window.UnregisterPostPollCallback(std::bind(&Renderer::OnWindowPostPoll, this));

		m_frameInfoBuffers.clear();
		m_lightBuffers.clear();
		m_frameInfoBufferData.clear();
		m_lightBufferData.clear();
		m_renderPasses.clear();
		m_computePasses.clear();

		m_nvidiaReflex.reset();
		m_renderGraph.reset();
		m_linearSampler.reset();
		m_nearestSampler.reset();
		m_shadowSampler.reset();
		m_reductionSampler.reset();
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

			if (!buffer.Initialise("frameInfoBuffer", GetDevice(), bufferSize, BufferUsageFlags::UniformBuffer, MemoryUsage::Auto,
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

	void Renderer::SetAsyncComputeState(bool enable)
	{
		if (!m_asyncComputeSupported && enable)
		{
			Logger::Error("Async compute is not supported.");
			return;
		}

		if (m_asyncComputeEnabled != enable)
		{
			m_asyncComputePendingState = enable;
			m_renderGraph->MarkDirty();
		}
	}

	void Renderer::SetAntiAliasingMode(AntiAliasingMode mode)
	{
		m_renderSettings.m_aaMode = mode;

		m_renderGraph->SetPassEnabled("FXAA", mode == AntiAliasingMode::FXAA);

		m_renderGraph->SetPassEnabled("SMAAEdges", mode == AntiAliasingMode::SMAA);
		m_renderGraph->SetPassEnabled("SMAAWeights", mode == AntiAliasingMode::SMAA);
		m_renderGraph->SetPassEnabled("SMAABlend", mode == AntiAliasingMode::SMAA);

		m_renderGraph->SetPassEnabled("TAA", mode == AntiAliasingMode::TAA);
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

			if (!buffer.Initialise("LightData", GetDevice(), bufferSize, BufferUsageFlags::UniformBuffer, MemoryUsage::Auto,
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
		frameInfo->jitter = m_renderSettings.m_aaMode == AntiAliasingMode::TAA ? m_postProcessing->GetTAAJitter() : glm::vec2();
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

		m_reductionSampler = std::move(m_resourceFactory->CreateImageSampler());
		if (!m_reductionSampler->Initialise(*m_device, Filter::Linear, Filter::Linear, SamplerMipmapMode::Nearest,
			SamplerAddressMode::ClampToEdge, 0.0f, SamplerCreationFlags::ReductionSampler))
		{
			return false;
		}

		if (!m_renderGraph->Initialise(*m_physicalDevice, *m_device, *m_resourceFactory, m_maxConcurrentFrames, m_asyncComputePendingState))
		{
			Logger::Error("Failed to initialise render graph.");
			return false;
		}

		m_renderPasses["SceneOpaque"] = std::make_unique<SceneOpaquePass>(*m_sceneGeometryBatch);
		m_renderPasses["SceneShadow"] = std::make_unique<SceneShadowPass>(*m_sceneGeometryBatch, *m_shadowMap);
		m_renderPasses["Combine"] = std::make_unique<CombinePass>(*m_shadowMap);

		m_computePasses["FrustumCulling"] = std::make_unique<FrustumCullingPass>(*m_sceneGeometryBatch);
		m_computePasses["ShadowCulling"] = std::make_unique<ShadowCullingPass>(*m_sceneGeometryBatch, *m_shadowMap);
		FrustumCullingPass& frustumCullingPass = reinterpret_cast<FrustumCullingPass&>(*m_computePasses["FrustumCulling"].get());
		m_computePasses["DepthReduction"] = std::make_unique<DepthReductionPass>(frustumCullingPass);

		for (const auto& pair : m_renderPasses)
		{
			if (!m_renderGraph->AddRenderNode(pair.second.get(), *m_materialManager))
			{
				Logger::Error("Failed to add render pass '{}' to render graph.", pair.first);
				return false;
			}
		}

		for (const auto& pair : m_computePasses)
		{
			if (!m_renderGraph->AddRenderNode(pair.second.get(), *m_materialManager))
			{
				Logger::Error("Failed to add compute pass '{}' to render graph.", pair.first);
				return false;
			}
		}

		std::vector<IRenderPass*> postProcessPasses = m_postProcessing->GetRenderPasses();
		for (IRenderPass* pass : postProcessPasses)
		{
			if (!m_renderGraph->AddRenderNode(pass, *m_materialManager))
			{
				Logger::Error("Failed to add post process pass to render graph.");
				return false;
			}
		}

		// Disable anti-aliasing passes that aren't active.
		SetAntiAliasingMode(m_renderSettings.m_aaMode);

		// Add UI pass after post processing.
		m_renderPasses["UI"] = std::make_unique<UIPass>(*m_uiManager);
		if (!m_renderGraph->AddRenderNode(m_renderPasses["UI"].get(), *m_materialManager))
		{
			Logger::Error("Failed to add pass 'UI' to render graph.");
			return false;
		}

		if (!m_renderGraph->Build(*this, m_asyncComputePendingState))
		{
			Logger::Error("Failed to build render graph.");
			return false;
		}

		return true;
	}

	void Renderer::OnWindowPrePoll()
	{
		m_nvidiaReflex->SetMarker(NvidiaReflexMarker::InputSample);
	}

	void Renderer::OnWindowPostPoll()
	{
		if (m_window.InputState.MouseButtonDown(MouseButton::Left))
			m_nvidiaReflex->SetMarker(NvidiaReflexMarker::TriggerFlash);
	}

	std::unique_ptr<Renderer> Renderer::Create(RendererType rendererType, Window& window, bool debug)
	{
		std::unique_ptr<Renderer> result;

		switch (rendererType)
		{
		case RendererType::Vulkan:
			result = std::make_unique<VulkanRenderer>(window, debug);
			break;
		default:
			Logger::Error("Requested renderer type not supported.");
			return nullptr;
		}

		window.RegisterPrePollCallback(std::bind(&Renderer::OnWindowPrePoll, result.get()));
		window.RegisterPostPollCallback(std::bind(&Renderer::OnWindowPostPoll, result.get()));

		return result;
	}

	void Renderer::SetDebugMode(uint32_t mode)
	{
		m_debugMode = mode;
		const std::unique_ptr<CombinePass>& combinePass = reinterpret_cast<const std::unique_ptr<CombinePass>&>(m_renderPasses.at("Combine"));
		combinePass->GetMaterial()->SetSpecialisationConstant("debugMode", static_cast<int32_t>(mode));
	}

	void Renderer::SetCullingMode(CullingMode mode)
	{
		const std::unique_ptr<FrustumCullingPass>& frustumCullingPass = reinterpret_cast<const std::unique_ptr<FrustumCullingPass>&>(m_computePasses.at("FrustumCulling"));
		const std::unique_ptr<ShadowCullingPass>& shadowCullingPass = reinterpret_cast<const std::unique_ptr<ShadowCullingPass>&>(m_computePasses.at("ShadowCulling"));
		frustumCullingPass->SetCullingMode(mode);
		shadowCullingPass->SetCullingMode(mode);

		m_renderGraph->SetPassEnabled("DepthReduction", mode == CullingMode::FrustumAndOcclusion);
	}

	void Renderer::SetHDRState(bool enable)
	{
		m_renderSettings.m_hdr = enable;

		const IRenderPass* pass;
		if (m_renderGraph->TryGetRenderPass("Tonemapper", &pass))
		{
			pass->GetMaterial()->SetSpecialisationConstant("isHdr", enable ? 1 : 0);
		}
	}

	void Renderer::SetShadowResolution(uint32_t resolution)
	{
		m_shadowMap->SetResolution(resolution);
		m_renderGraph->MarkDirty();
	}

	bool Renderer::BeginFrame() const
	{
		// Nvidia Reflex hangs on sleep command when VK_LAYER_KHRONOS_validation is enabled.
		//if (!m_nvidiaReflex->Sleep())
		//	return false;

		m_nvidiaReflex->SetMarker(NvidiaReflexMarker::SimulationStart);

		return true;
	}

	bool Renderer::Render()
	{
		m_nvidiaReflex->SetMarker(NvidiaReflexMarker::SimulationEnd);

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