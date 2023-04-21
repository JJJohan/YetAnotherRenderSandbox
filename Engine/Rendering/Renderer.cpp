#include "Renderer.hpp"
#include "Core/Logging/Logger.hpp"
#include "Vulkan/VulkanRenderer.hpp"
#include "OS/Window.hpp"

using namespace Engine::OS;
using namespace Engine::Logging;

namespace Engine::Rendering
{
	Renderer::Renderer(bool debug)
		: m_debug(debug)
	{
	}

	Renderer::~Renderer()
	{
	}

	std::unique_ptr<Renderer> Renderer::Create(RendererType rendererType, bool debug)
	{
		switch (rendererType)
		{
		case RendererType::VULKAN:
			return std::make_unique<VulkanRenderer>(debug);
		default:
			Logger::Error("Requested renderer type not supported.");
			return nullptr;
		}
	}

	bool Renderer::Initialise(const Window& window)
	{
		return true;
	}

	void Renderer::Render()
	{
	}
}