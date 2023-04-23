#include <Core/Logging/Logger.hpp>
#include <OS/Window.hpp>
#include <OS/Files.hpp>
#include <Rendering/Renderer.hpp>
#include <Rendering/Shader.hpp>

using namespace Engine::Rendering;
using namespace Engine::Logging;
using namespace Engine::OS;

#ifndef NDEBUG
const bool debug = true;
#else
const bool debug = false;
#endif

int main()
{
    Logger::SetLogOutputLevel(LogLevel::VERBOSE);

    std::unique_ptr<Window> window = Window::Create("Test", 1280, 720, false);
    Renderer* renderer = window->CreateRenderer(RendererType::VULKAN, debug);

    renderer->SetClearColour(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));

    if (!renderer || !renderer->Initialise())
    {
        Logger::Error("Failed to initialise renderer.");
        return 1;
    }

    Shader* shader = renderer->CreateShader("Triangle", {
        { ShaderProgramType::VERTEX, "Shaders/Triangle_vert.spv" },
        { ShaderProgramType::FRAGMENT, "Shaders/Triangle_frag.spv" }
        });

    if (!shader)
    {
        Logger::Error("Shader invalid.");
        return 1;
    }

    while (!window->IsClosed())
    {
        renderer->Render();
        window->Poll();
    }

    return 0;
}