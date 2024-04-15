#pragma once

#include <string>
#include <memory>
#include "Core/Macros.hpp"
#include "Core/Logging/Logger.hpp"
#include "InputState.hpp"
#include <functional>

namespace Engine::OS
{
	struct MonitorInfo
	{
		std::string DeviceName;
		uint32_t BitsPerColor;
		float RedPrimary[2];
		float GreenPrimary[2];
		float BluePrimary[2];
		float WhitePoint[2];
		float MinLuminance;
		float MaxLuminance;
		float MaxFullFrameLuminance;
	};

	class Window
	{
	private:
		template <typename TReturn>
		static inline void RegisterCallback(std::vector<std::function<TReturn(void)>>& collection, std::function<TReturn(void)> callback)
		{
			const auto& target = callback.target<TReturn(void)>();
			for (auto it = collection.cbegin(); it != collection.cend(); ++it)
			{
				if (it->target<TReturn(void)>() == target)
				{
					Engine::Logging::Logger::Error("Callback already registered.");
					return;
				}
			}

			collection.push_back(callback);
		}

		template <typename TReturn>
		static inline void UnregisterCallback(std::vector<std::function<TReturn(void)>>& collection, std::function<TReturn(void)> callback)
		{
			const auto& target = callback.target<TReturn(void)>();
			for (auto it = collection.cbegin(); it != collection.cend(); ++it)
			{
				if (it->target<TReturn(void)>() == target)
				{
					collection.erase(it);
					return;
				}
			}

			Engine::Logging::Logger::Error("Callback was not registered.");
		}

		template <typename TReturn, class TArg>
		static inline void RegisterCallback(std::vector<std::function<TReturn(TArg)>>& collection, std::function<TReturn(TArg)> callback)
		{
			const auto& target = callback.target<TReturn(TArg)>();
			for (auto it = collection.cbegin(); it != collection.cend(); ++it)
			{
				if (it->target<TReturn(TArg)>() == target)
				{
					Engine::Logging::Logger::Error("Callback already registered.");
					return;
				}
			}

			collection.push_back(callback);
		}

		template <typename TReturn, typename TArg>
		static inline void UnregisterCallback(std::vector<std::function<TReturn(TArg)>>& collection, std::function<TReturn(TArg)> callback)
		{
			const auto& target = callback.target<TReturn(TArg)>();
			for (auto it = collection.cbegin(); it != collection.cend(); ++it)
			{
				if (it->target<TReturn(TArg)>() == target)
				{
					collection.erase(it);
					return;
				}
			}

			Engine::Logging::Logger::Error("Callback was not registered.");
		}

	public:
		EXPORT static std::unique_ptr<Window> Create(const std::string& title, const glm::uvec2& size, bool fullscreen);
		EXPORT virtual ~Window();

		inline bool IsCursorVisible() const { return m_cursorVisible; }
		inline const std::string& GetTitle() const { return m_title; }
		inline const glm::uvec2 GetSize() const { return m_size; }
		inline bool IsFullscreen() const { return m_fullscreen; }
		inline bool IsClosed() const { return m_closed; }
		inline virtual void* GetHandle() const { return nullptr; }
		inline virtual void* GetInstance() const { return nullptr; }

		inline virtual void SetCursorVisible(bool visible) { m_cursorVisible = visible; }
		inline virtual void SetTitle(const std::string& title) { m_title = title; }
		inline virtual void SetFullscreen(bool fullscreen) { m_fullscreen = fullscreen; }
		EXPORT virtual void Resize(const glm::uvec2& size);
		EXPORT virtual void Close();
		EXPORT virtual void Poll();
		EXPORT virtual bool QueryMonitorInfo(MonitorInfo& info) const;

		inline void RegisterPrePollCallback(std::function<void(void)> callback) { RegisterCallback(m_prePollCallbacks, callback); }
		inline void RegisterPostPollCallback(std::function<void(void)> callback) { RegisterCallback(m_postPollCallbacks, callback); }
		inline void UnregisterPrePollCallback(std::function<void(void)> callback) { UnregisterCallback(m_prePollCallbacks, callback); }
		inline void UnregisterPostPollCallback(std::function<void(void)> callback) { UnregisterCallback(m_postPollCallbacks, callback); }

		inline void RegisterResizeCallback(std::function<void(const glm::uvec2&)> callback) { RegisterCallback(m_resizeCallbacks, callback); }
		inline void UnregisterResizeCallback(std::function<void(const glm::uvec2&)> callback) { UnregisterCallback(m_resizeCallbacks, callback); }

		inline void RegisterCloseCallback(std::function<void(void)> callback) { RegisterCallback(m_closeCallbacks, callback); }
		inline void UnregisterCloseCallback(std::function<void(void)> callback) { RegisterCallback(m_closeCallbacks, callback); }

		void OnResize(const glm::uvec2& size);

		InputState InputState;

	protected:
		Window(const std::string& name, const glm::uvec2& size, bool fullscreen);

		std::vector<std::function<void(void)>> m_prePollCallbacks;
		std::vector<std::function<void(void)>> m_postPollCallbacks;
		std::vector<std::function<void(const glm::uvec2&)>> m_resizeCallbacks;
		std::vector<std::function<void(void)>> m_closeCallbacks;
		std::string m_title;
		std::atomic<glm::uvec2> m_size;
		bool m_fullscreen;
		bool m_closed;
		bool m_cursorVisible;
	};
}