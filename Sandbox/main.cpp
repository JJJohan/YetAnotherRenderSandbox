#include <Core/Logging/Logger.hpp>
#include <OS/Window.hpp>
#include <Rendering/Renderer.hpp>

using namespace Engine::Rendering;
using namespace Engine::Logging;
using namespace Engine::OS;

const bool debug = true;

int main()
{
    Logger::SetLogOutputLevel(LogLevel::VERBOSE);

    std::unique_ptr<Window> window = Window::Create("Test", 1280, 720, false);

    std::unique_ptr<Renderer> renderer = Renderer::Create(RendererType::VULKAN, debug);

    if (!renderer->Initialise(*window))
    {
        Logger::Log(LogLevel::FATAL, "Failed to initialise renderer.");
        return 1;
    }

    while (!window->IsClosed())
    {
        window->Poll();
    }

    return 0;
}