#pragma once

#include <memory>
#include <vector>
#include "Core/Colour.hpp"
#include "Core/Macros.hpp"
#include <glm/glm.hpp>
#include "Camera.hpp"
#include "RenderStats.hpp"
#include "Types.hpp"
#include "Resources/ISwapChain.hpp"
#include "RenderGraph.hpp"

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
	class GBuffer;
	class ShadowMap;
	class PostProcessing;
	class RenderStats;
	class IDevice;
	class IPhysicalDevice;
	class IMaterialManager;
	struct FrameInfoUniformBuffer;
	struct LightUniformBuffer;
	class IBuffer;
	class ISwapChain;
	class IRenderPass;
	class IGeometryBatch;

	enum class RendererType
	{
		VULKAN
	};

	class Renderer
	{
	public:
		EXPORT static std::unique_ptr<Renderer> Create(RendererType rendererType, const Engine::OS::Window& window, bool debug);
		EXPORT virtual ~Renderer();

		inline virtual bool Initialise();
		inline virtual bool Render() = 0;
		inline const std::vector<FrameStats>& GetRenderStats() const { return m_renderStats->GetFrameStats(); };
		inline const MemoryStats& GetMemoryStats() const { return m_renderStats->GetMemoryStats(); };

		inline void SetClearColour(const Colour& clearColour) { m_clearColour = clearColour.GetVec4(); };
		inline const Colour GetClearColor() const { return Colour(m_clearColour); };

		inline virtual void SetTemporalAAState(bool enabled) { m_temporalAA = enabled; };
		inline bool GetTemporalAAState(bool enabled) const { return m_temporalAA; };

		inline virtual void SetDebugMode(uint32_t mode) { m_debugMode = mode; };
		inline uint32_t GetDebugMode() const { return m_debugMode; };

		inline virtual void SetMultiSampleCount(uint32_t multiSampleCount);;
		inline uint32_t GetMaxMultiSampleCount() const { return m_maxMultiSampleCount; };

		inline virtual void SetHDRState(bool enable) { m_hdr = enable; };
		inline bool GetHDRState() const { return m_hdr; };
		bool IsHDRSupported() const { return m_swapChain->IsHDRCapable(); };

		inline void SetSunLightDirection(const glm::vec3& dir) { m_sunDirection = glm::normalize(dir); };
		inline void SetSunLightColour(const Colour& colour) { m_sunColour = colour; };
		inline void SetSunLightIntensity(float intensity) { m_sunIntensity = intensity; };

		inline const GBuffer& GetGBuffer() const { return *m_gBuffer; };

		inline void SetCamera(const Camera& camera) { m_camera = camera; };
		inline Camera& GetCamera() { return m_camera; };

		inline const IResourceFactory& GetResourceFactory() const { return *m_resourceFactory; }

		inline SceneManager& GetSceneManager() const { return *m_sceneManager; };
		inline Engine::UI::UIManager& GetUIManager() const { return *m_uiManager; };

		inline const IDevice& GetDevice() const { return *m_device; };
		inline const IPhysicalDevice& GetPhysicalDevice() const { return *m_physicalDevice; };
		inline const ISwapChain& GetSwapChain() const { return *m_swapChain; };
		inline uint32_t GetConcurrentFrameCount() const { return m_maxConcurrentFrames; };

		inline const std::vector<std::unique_ptr<IBuffer>>& GetFrameInfoBuffers() const { return m_frameInfoBuffers; };
		inline const std::vector<std::unique_ptr<IBuffer>>& GetLightBuffers() const { return m_lightBuffers; };

		virtual bool PrepareSceneGeometryBatch(IGeometryBatch** geometryBatch) = 0;

	protected:
		Renderer(const Engine::OS::Window& window, bool debug);

		bool CreateFrameInfoUniformBuffer();
		bool CreateLightUniformBuffer();
		void UpdateFrameInfo();

		virtual void DestroyResources();

		uint32_t m_currentFrame;
		glm::uvec2 m_lastWindowSize;
		uint32_t m_maxConcurrentFrames;
		glm::vec3 m_sunDirection;
		Colour m_sunColour;
		float m_sunIntensity;
		uint32_t m_debugMode;
		uint32_t m_multiSampleCount;
		uint32_t m_maxMultiSampleCount;
		const Engine::OS::Window& m_window;
		bool m_temporalAA;
		bool m_debug;
		bool m_hdr;
		Camera m_camera;
		glm::vec4 m_clearColour;
		Format m_depthFormat;

		std::vector<std::unique_ptr<IBuffer>> m_frameInfoBuffers;
		std::vector<std::unique_ptr<IBuffer>> m_lightBuffers;
		std::vector<FrameInfoUniformBuffer*> m_frameInfoBufferData;
		std::vector<LightUniformBuffer*> m_lightBufferData;
		std::vector<std::unique_ptr<IRenderPass>*> m_renderPasses;
		std::unique_ptr<RenderGraph> m_renderGraph;

		std::unique_ptr<SceneManager> m_sceneManager;
		std::unique_ptr<IGeometryBatch> m_sceneGeometryBatch;
		std::unique_ptr<IResourceFactory> m_resourceFactory;
		std::unique_ptr<GBuffer> m_gBuffer;
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