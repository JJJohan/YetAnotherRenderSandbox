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

	void Renderer::SetMultiSampleCount(uint32_t multiSampleCount)
	{
		if (multiSampleCount > m_maxMultiSampleCount)
		{
			Logger::Error("Sample count of {} exceeds maximum supported value of {}.", multiSampleCount, m_maxMultiSampleCount);
			return;
		}

		m_multiSampleCount = std::clamp(multiSampleCount, 1U, m_maxMultiSampleCount);
	}
}