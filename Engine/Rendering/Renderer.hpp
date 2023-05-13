#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include "Core/Colour.hpp"
#include "Core/Macros.hpp"
#include "Shader.hpp"
#include <glm/glm.hpp>
#include "Camera.hpp"

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

	enum class RendererType
	{
		VULKAN
	};

	class Renderer
	{
	public:
		EXPORT static std::unique_ptr<Renderer> Create(RendererType rendererType, const Engine::OS::Window& window, bool debug);
		EXPORT virtual ~Renderer();

		EXPORT virtual bool Initialise();
		EXPORT virtual bool Render();

		EXPORT void SetClearColour(const Colour& colour);
		EXPORT const Colour GetClearColor() const;

		EXPORT virtual void SetMultiSampleCount(uint32_t multiSampleCount);
		EXPORT uint32_t GetMaxMultiSampleCount() const;

		EXPORT void SetSunLightDirection(const glm::vec3& dir);
		EXPORT void SetSunLightColour(const Colour& colour);

		EXPORT void SetCamera(const Camera& camera);
		EXPORT Camera& GetCamera();

		EXPORT virtual Shader* CreateShader(const std::string& name, const std::unordered_map<ShaderProgramType, std::vector<uint8_t>>& programs);
		EXPORT Shader* CreateShader(const std::string& name, const std::unordered_map<ShaderProgramType, std::string>& programs);
		EXPORT virtual void DestroyShader(Shader* shader);

		EXPORT virtual SceneManager* GetSceneManager() const;
		EXPORT virtual Engine::UI::UIManager* GetUIManager() const;

	protected:
		Renderer(const Engine::OS::Window& window, bool debug);

		glm::vec3 m_sunDirection;
		Colour m_sunColour;
		uint32_t m_multiSampleCount;
		uint32_t m_maxMultiSampleCount;
		const Engine::OS::Window& m_window;
		bool m_debug;
		Camera m_camera;
		glm::vec4 m_clearColour;
	};
}