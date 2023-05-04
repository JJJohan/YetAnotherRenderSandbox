#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include "Core/Macros.hpp"
#include "Shader.hpp"
#include <glm/glm.hpp>
#include "Camera.hpp"

namespace Engine::OS
{
	class Window;
}

namespace Engine::Rendering
{
	class MeshManager;
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
		EXPORT virtual void Render();

		EXPORT void SetClearColour(const glm::vec4& colour);
		EXPORT const glm::vec4& GetClearColor() const;

		EXPORT virtual void SetMultiSampleCount(uint32_t multiSampleCount);
		EXPORT uint32_t GetMaxMultiSampleCount() const;

		EXPORT void SetCamera(const Camera& camera);
		EXPORT Camera& GetCamera();

		EXPORT virtual Shader* CreateShader(const std::string& name, const std::unordered_map<ShaderProgramType, std::vector<uint8_t>>& programs);
		EXPORT Shader* CreateShader(const std::string& name, const std::unordered_map<ShaderProgramType, std::string>& programs);
		EXPORT virtual void DestroyShader(Shader* shader);

		EXPORT virtual MeshManager* GetMeshManager() const;

	protected:
		Renderer(const Engine::OS::Window& window, bool debug);

		uint32_t m_multiSampleCount;
		uint32_t m_maxMultiSampleCount;
		const Engine::OS::Window& m_window;
		bool m_debug;
		Camera m_camera;
		glm::vec4 m_clearColour;
	};
}