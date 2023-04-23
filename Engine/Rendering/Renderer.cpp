#include "Renderer.hpp"
#include "Core/Logging/Logger.hpp"
#include "Vulkan/VulkanRenderer.hpp"
#include "OS/Window.hpp"
#include "OS/Files.hpp"

using namespace Engine::OS;
using namespace Engine::Logging;
using namespace Engine::Rendering::Vulkan;

namespace Engine::Rendering
{
	Renderer::Renderer(const Window& window, bool debug)
		: m_debug(debug)
		, m_window(window)
		, m_resized(false)
		, m_clearColour()
	{
	}

	Renderer::~Renderer()
	{
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

	void Renderer::NotifyResizeEvent()
	{
		m_resized = true;
	}

	void Renderer::SetClearColour(const glm::vec4& clearColour)
	{
		m_clearColour = clearColour;
	}

	const glm::vec4& Renderer::GetClearColor() const
	{
		return m_clearColour;
	}

	Shader* Renderer::CreateShader(const std::string& name, const std::unordered_map<ShaderProgramType, std::vector<uint8_t>>& programs)
	{
		return nullptr;
	}

	Shader* Renderer::CreateShader(const std::string& name, const std::unordered_map<ShaderProgramType, std::string>& programInfos)
	{
		std::unordered_map<ShaderProgramType, std::vector<uint8_t>> programs{};
		programs.reserve(programInfos.size());
		for (const auto& programInfo : programInfos)
		{
			std::vector<uint8_t> data;
			if (!Files::TryReadFile(programInfo.second, data))
			{
				Logger::Error("Failed to read shader program at path '{}'.", programInfo.second);
				return nullptr;;
			}

			programs.emplace(programInfo.first, std::move(data));
		}

		return CreateShader(name, programs);
	}

	void Renderer::DestroyShader(Shader* shader)
	{
	}

	bool Renderer::Initialise()
	{
		return true;
	}

	void Renderer::Shutdown()
	{
	}

	void Renderer::Render()
	{
	}
}