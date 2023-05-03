#include "Renderer.hpp"
#include "Core/Logging/Logger.hpp"
#include "Vulkan/VulkanRenderer.hpp"
#include "MeshManager.hpp"
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
		, m_clearColour()
		, m_camera()
		, m_sampleCount(1)
		, m_maxSampleCount(1)
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

	void Renderer::SetCamera(const Camera& camera)
	{
		m_camera = camera;
	}

	Camera& Renderer::GetCamera()
	{
		return m_camera;
	}

	MeshManager* Renderer::GetMeshManager() const
	{
		return nullptr;
	}

	void Renderer::SetSampleCount(uint32_t sampleCount)
	{
		if (sampleCount > m_maxSampleCount)
		{
			Logger::Error("Sample count of {} exceeds maximum supported value of {}.", sampleCount, m_maxSampleCount);
			return;
		}

		m_sampleCount = std::clamp(sampleCount, 1U, m_maxSampleCount);
	}

	uint32_t Renderer::GetMaxSampleCount() const
	{
		return m_maxSampleCount;
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
				return nullptr;
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

	void Renderer::Render()
	{
	}
}