#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include "Core/Colour.hpp"
#include "Core/Macros.hpp"
#include <glm/glm.hpp>
#include "Camera.hpp"
#include "RenderStats.hpp"

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
	class SceneManager;
	class Camera;
	class IResourceFactory;

	enum class RendererType
	{
		VULKAN
	};

	class Renderer
	{
	public:
		EXPORT static std::unique_ptr<Renderer> Create(RendererType rendererType, const Engine::OS::Window& window, bool debug);
		EXPORT virtual ~Renderer();

		inline virtual bool Initialise() { return true; }
		inline virtual bool Render() { return false; }
		EXPORT virtual const std::vector<FrameStats>& GetRenderStats() const { throw; };
		EXPORT virtual const MemoryStats& GetMemoryStats() const { throw; };

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
		inline virtual bool IsHDRSupported() const { return false; };

		inline void SetSunLightDirection(const glm::vec3& dir) { m_sunDirection = glm::normalize(dir); };
		inline void SetSunLightColour(const Colour& colour) { m_sunColour = colour; };
		inline void SetSunLightIntensity(float intensity) { m_sunIntensity = intensity; };

		inline void SetCamera(const Camera& camera) { m_camera = camera; };
		inline Camera& GetCamera() { return m_camera; };

		inline const IResourceFactory& GetResourceFactory() { return *m_resourceFactory; }

		inline virtual SceneManager& GetSceneManager() const { throw; };
		inline virtual Engine::UI::UIManager& GetUIManager() const { throw; };

	protected:
		Renderer(const Engine::OS::Window& window, bool debug);

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
		std::unique_ptr<IResourceFactory> m_resourceFactory;
	};
}