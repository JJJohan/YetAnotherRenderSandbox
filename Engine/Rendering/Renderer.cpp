#include "Renderer.hpp"
#include "Core/Logging/Logger.hpp"
#include "Vulkan/VulkanRenderer.hpp"
#include "SceneManager.hpp"
#include "OS/Window.hpp"
#include "OS/Files.hpp"
#include "UI/UIManager.hpp"

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
		, m_multiSampleCount(1)
		, m_maxMultiSampleCount(1)
		, m_sunDirection(glm::normalize(glm::vec3(0.2f, -1.0f, 2.0f)))
		, m_sunColour(Colour(1.0f, 1.0f, 1.0f))
		, m_sunIntensity(1.0f)
		, m_debugMode(0)
		, m_hdr(false)
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

	void Renderer::SetSunLightDirection(const glm::vec3& dir)
	{
		m_sunDirection = glm::normalize(dir);
	}

	void Renderer::SetSunLightColour(const Colour& colour)
	{
		m_sunColour = colour;
	}

	void Renderer::SetSunLightIntensity(float intensity)
	{
		m_sunIntensity = intensity;
	}

	void Renderer::SetCamera(const Camera& camera)
	{
		m_camera = camera;
	}

	Camera& Renderer::GetCamera()
	{
		return m_camera;
	}

	SceneManager* Renderer::GetSceneManager() const
	{
		return nullptr;
	}

	UIManager* Renderer::GetUIManager() const
	{
		return nullptr;
	}

	void Renderer::SetMultiSampleCount(uint32_t multiSampleCount)
	{
		if (multiSampleCount > m_maxMultiSampleCount)
		{
			Logger::Error("Sample count of {} exceeds maximum supported value of {}.", multiSampleCount, m_maxMultiSampleCount);
			return;
		}

		m_multiSampleCount = std::clamp(multiSampleCount, 1U, m_maxMultiSampleCount);
	}

	uint32_t Renderer::GetMaxMultiSampleCount() const
	{
		return m_maxMultiSampleCount;
	}

	void Renderer::SetClearColour(const Colour& clearColour)
	{
		m_clearColour = clearColour.GetVec4();
	}

	const Colour Renderer::GetClearColor() const
	{
		return Colour(m_clearColour);
	}

	void Renderer::SetHDRState(bool enable)
	{
		m_hdr = enable;
	}

	bool Renderer::GetHDRState() const
	{
		return m_hdr;
	}

	bool Renderer::IsHDRSupported() const
	{
		return false;
	}

	void Renderer::SetDebugMode(uint32_t mode)
	{
		m_debugMode = mode;
	}

	uint32_t Renderer::GetDebugMode() const
	{
		return m_debugMode;
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

	bool Renderer::Render()
	{
		return false;
	}
}