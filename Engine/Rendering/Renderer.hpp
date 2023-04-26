#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include "Core/Macros.hpp"
#include "OS/Window.hpp"
#include "Shader.hpp"
#include <glm/glm.hpp>

namespace Engine::OS
{
	class Window;
}

namespace Engine::Rendering
{
	class MeshManager;

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

		EXPORT virtual Shader* CreateShader(const std::string& name, const std::unordered_map<ShaderProgramType, std::vector<uint8_t>>& programs);
		EXPORT Shader* CreateShader(const std::string& name, const std::unordered_map<ShaderProgramType, std::string>& programs);
		EXPORT virtual void DestroyShader(Shader* shader);

		EXPORT virtual MeshManager* GetMeshManager() const;

	protected:
		Renderer(const Engine::OS::Window& window, bool debug);

		const Engine::OS::Window& m_window;
		bool m_debug;
		glm::vec4 m_clearColour;
	};
}