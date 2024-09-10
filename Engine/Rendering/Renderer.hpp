#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include "Core/Colour.hpp"
#include "Core/Macros.hpp"
#include <glm/glm.hpp>
#include "Camera.hpp"
#include "RenderStats.hpp"
#include "Types.hpp"
#include "ISwapChain.hpp"
#include "RenderGraph.hpp"
#include "RenderSettings.hpp"
#include "Resources/SubmitInfo.hpp"
#include <functional>
#include "CullingMode.hpp"
#include "AntiAliasingMode.hpp"
#include "NvidiaReflex.hpp"

namespace Engine
{
	class SceneManager;
}

namespace Engine::OS
{
	class Window;
}

namespace Engine::UI
{
	class UIManager;
}

namespace Engine::Rendering
{
	class Camera;
	class IResourceFactory;
	class ShadowMap;
	class PostProcessing;
	class IDevice;
	class IPhysicalDevice;
	class IMaterialManager;
	struct FrameInfoUniformBuffer;
	struct LightUniformBuffer;
	class IBuffer;
	class ISwapChain;
	class IRenderPass;
	class IComputePass;
	class IImageSampler;
	class GeometryBatch;

	enum class RendererType
	{
		Vulkan
	};

	class Renderer
	{
	public:
		EXPORT static std::unique_ptr<Renderer> Create(RendererType rendererType, Engine::OS::Window& window, bool debug);
		EXPORT virtual ~Renderer();

		inline virtual bool Initialise();
		EXPORT bool BeginFrame() const;
		inline virtual bool Render();
		inline const std::unordered_map<std::string, FrameStats>& GetRenderStats() const { return m_renderStats->GetFrameStats(); }
		inline const MemoryStats& GetMemoryStats() const { return m_renderStats->GetMemoryStats(); }
		inline RenderGraph& GetRenderGraph() const { return *m_renderGraph; }

		inline void SetClearColour(const Colour& clearColour) { m_clearColour = clearColour.GetVec4(); }
		inline const Colour GetClearColor() const { return Colour(m_clearColour); }

		EXPORT void SetAntiAliasingMode(AntiAliasingMode mode);
		inline AntiAliasingMode GetTemporalAAState(bool enabled) const { return m_renderSettings.m_aaMode; }

		EXPORT void SetDebugMode(uint32_t mode);
		inline uint32_t GetDebugMode() const { return m_debugMode; }

		inline NvidiaReflex& NvidiaReflex() const { return *m_nvidiaReflex; }

		EXPORT void SetCullingMode(CullingMode mode);

		inline virtual void SetMultiSampleCount(uint32_t multiSampleCount);;
		inline uint32_t GetMaxMultiSampleCount() const { return m_maxMultiSampleCount; }

		EXPORT virtual void SetHDRState(bool enable);
		inline bool GetHDRState() const { return m_renderSettings.m_hdr; }
		bool IsHDRSupported() const { return m_swapChain->IsHDRCapable(); }

		EXPORT void SetAsyncComputeState(bool enable);
		inline bool GetAsyncComputeState() const { return m_asyncComputeEnabled; }
		inline bool IsAsyncComputeSupported() const { return m_asyncComputeSupported; }

		inline void SetSunLightDirection(const glm::vec3& dir) { m_sunDirection = glm::normalize(dir); }
		inline void SetSunLightColour(const Colour& colour) { m_sunColour = colour; }
		inline void SetSunLightIntensity(float intensity) { m_sunIntensity = intensity; }

		EXPORT void SetShadowResolution(uint32_t resolution);

		inline const ShadowMap& GetShadowMap() const { return *m_shadowMap; }

		inline void SetCamera(const Camera& camera) { m_camera = camera; }
		inline Camera& GetCamera() { return m_camera; }
		inline const Camera& GetCameraReadOnly() const { return m_camera; }

		inline const IResourceFactory& GetResourceFactory() const { return *m_resourceFactory; }

		inline SceneManager& GetSceneManager() const { return *m_sceneManager; }
		inline IMaterialManager& GetMaterialManager() const { return *m_materialManager; }
		inline Engine::UI::UIManager& GetUIManager() const { return *m_uiManager; }

		inline const IDevice& GetDevice() const { return *m_device; }
		inline const IPhysicalDevice& GetPhysicalDevice() const { return *m_physicalDevice; }
		inline const ISwapChain& GetSwapChain() const { return *m_swapChain; }
		inline uint32_t GetConcurrentFrameCount() const { return m_maxConcurrentFrames; }
		inline Format GetDepthFormat() const { return m_depthFormat; }

		inline const std::vector<std::unique_ptr<IBuffer>>& GetFrameInfoBuffers() const { return m_frameInfoBuffers; }
		inline const std::vector<std::unique_ptr<IBuffer>>& GetLightBuffers() const { return m_lightBuffers; }

		inline const IImageSampler& GetLinearSampler() const { return *m_linearSampler; }
		inline const IImageSampler& GetNearestSampler() const { return *m_nearestSampler; }
		inline const IImageSampler& GetShadowSampler() const { return *m_shadowSampler; }
		inline const IImageSampler& GetReductionSampler() const { return *m_reductionSampler; }

		inline GeometryBatch& GetSceneGeometryBatch() const { return *m_sceneGeometryBatch; }

		inline IRenderImage& GetBlankImage() const { return *m_blankImage; }
		inline IRenderImage& GetBlankShadowImage() const { return *m_blankShadowImage; }

		virtual bool SubmitResourceCommand(std::function<bool(const IDevice& device, const IPhysicalDevice& physicalDevice,
			const ICommandBuffer&, std::vector<std::unique_ptr<IBuffer>>&)> command,
			std::optional<std::function<void()>> postAction = std::nullopt) = 0;

		virtual IRenderImage& GetPresentImage() const = 0;

		virtual bool Present(const std::vector<SubmitInfo>& renderSubmitInfos, const std::vector<SubmitInfo>& computeSubmitInfos) = 0;

	protected:
		Renderer(Engine::OS::Window& window, bool debug);

		bool CreateFrameInfoUniformBuffer();
		bool CreateLightUniformBuffer();
		void UpdateFrameInfo();

		void OnWindowPrePoll();
		void OnWindowPostPoll();

		virtual void DestroyResources();

		uint32_t m_currentFrame;
		glm::uvec2 m_lastWindowSize;
		uint32_t m_maxConcurrentFrames;
		glm::vec3 m_sunDirection;
		Colour m_sunColour;
		float m_sunIntensity;
		uint32_t m_debugMode;
		uint32_t m_maxMultiSampleCount;
		Engine::OS::Window& m_window;
		bool m_debug;
		Camera m_camera;
		glm::vec4 m_clearColour;
		Format m_depthFormat;
		RenderSettings m_renderSettings;
		bool m_asyncComputeSupported;
		bool m_asyncComputeEnabled;
		bool m_asyncComputePendingState;

		std::vector<std::unique_ptr<IBuffer>> m_frameInfoBuffers;
		std::vector<std::unique_ptr<IBuffer>> m_lightBuffers;
		std::vector<FrameInfoUniformBuffer*> m_frameInfoBufferData;
		std::vector<LightUniformBuffer*> m_lightBufferData;
		std::unordered_map<std::string, std::unique_ptr<IRenderPass>> m_renderPasses;
		std::unordered_map<std::string, std::unique_ptr<IComputePass>> m_computePasses;
		std::unique_ptr<RenderGraph> m_renderGraph;
		std::unique_ptr<IImageSampler> m_linearSampler;
		std::unique_ptr<IImageSampler> m_nearestSampler;
		std::unique_ptr<IImageSampler> m_shadowSampler;
		std::unique_ptr<IImageSampler> m_reductionSampler;
		std::unique_ptr<IRenderImage> m_blankImage;
		std::unique_ptr<IRenderImage> m_blankShadowImage;
		std::unique_ptr<Engine::Rendering::NvidiaReflex> m_nvidiaReflex;

		std::unique_ptr<SceneManager> m_sceneManager;
		std::unique_ptr<GeometryBatch> m_sceneGeometryBatch;
		std::unique_ptr<IResourceFactory> m_resourceFactory;
		std::unique_ptr<ShadowMap> m_shadowMap;
		std::unique_ptr<PostProcessing> m_postProcessing;
		std::unique_ptr<IMaterialManager> m_materialManager;
		std::unique_ptr<IPhysicalDevice> m_physicalDevice;
		std::unique_ptr<IDevice> m_device;
		std::unique_ptr<ISwapChain> m_swapChain;
		std::unique_ptr<RenderStats> m_renderStats;
		std::unique_ptr<Engine::UI::UIManager> m_uiManager;
	};
}