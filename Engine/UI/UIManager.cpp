#include "UIManager.hpp"
#include "Rendering/Renderer.hpp"
#include "OS/Window.hpp"
#include "Core/Logger.hpp"
#include "imgui.h"
#include <functional>

#ifdef _WIN32
#include "backends/imgui_impl_win32.h"
#else
#error Not implemented.
#endif

using namespace Engine::OS;
using namespace Engine::Rendering;

namespace Engine::UI
{
	UIManager::UIManager(Window& window, Renderer& renderer)
		: m_window(window)
		, m_renderer(renderer)
		, m_drawCallbacks()
		, m_drawer()
		, m_initialised(false)
	{
	}

	UIManager::~UIManager()
	{
		if (m_initialised)
		{
			m_window.UnregisterDPIChangeCallback(std::bind(&UIManager::OnDPIChanged, this, std::placeholders::_1));
			m_initialised = false;
		}

#ifdef _WIN32
		ImGui_ImplWin32_Shutdown();
#else
#error Not implemented.
#endif
	}

	bool UIManager::Initialise()
	{
		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();

#ifdef _WIN32
		if (!ImGui_ImplWin32_Init(m_window.GetHandle()))
		{
			return false;
		}
#else
#error Not implemented.
#endif

		IM_DELETE(io.Fonts);

		io.Fonts = IM_NEW(ImFontAtlas);

		ImFontConfig config;
		config.OversampleH = 4;
		config.OversampleV = 4;
		config.PixelSnapH = false;

		io.FontDefault = io.Fonts->AddFontFromFileTTF("Fonts/Play-Regular.ttf", 16.0f, &config);

		io.Fonts->Build();

		OnDPIChanged(m_window.GetDPI());
		m_window.RegisterDPIChangeCallback(std::bind(&UIManager::OnDPIChanged, this, std::placeholders::_1));
		m_initialised = true;

		return true;
	}

	void UIManager::OnDPIChanged(uint32_t dpi)
	{
		float scale = static_cast<float>(dpi) / 96.0f;
		ImGuiIO& io = ImGui::GetIO();
		io.FontGlobalScale = scale;
	}

	void UIManager::RegisterDrawCallback(std::function<void(const Drawer&)> callback)
	{
		const auto& target = callback.target<void(const Drawer&)>();
		for (auto it = m_drawCallbacks.cbegin(); it != m_drawCallbacks.cend(); ++it)
		{
			if (it->target<void(const Drawer&)>() == target)
			{
				Logger::Error("Callback already registered.");
				return;
			}
		}

		m_drawCallbacks.push_back(callback);
	}

	void UIManager::UnregisterDrawCallback(std::function<void(const Drawer&)> callback)
	{
		const auto& target = callback.target<void(const Drawer&)>();
		for (auto it = m_drawCallbacks.cbegin(); it != m_drawCallbacks.cend(); ++it)
		{
			if (it->target<void(const Drawer&)>() == target)
			{
				m_drawCallbacks.erase(it);
				return;
			}
		}

		Logger::Error("Callback was not registered.");
	}

	float UIManager::GetFPS() const
	{
		return ImGui::GetIO().Framerate;
	}

	bool UIManager::Draw(float width, float height) const
	{
		if (!m_window.IsCursorVisible())
			return false;

		ImGuiIO& io = ImGui::GetIO();
		io.DisplaySize.x = width;
		io.DisplaySize.y = height;

		if (width < FLT_MIN || height < FLT_MIN)
			return false;

#ifdef _WIN32
		ImGui_ImplWin32_NewFrame();
#else
#error Not implemented.
#endif

		return true;
	}
}