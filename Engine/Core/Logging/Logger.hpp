#pragma once

#include <format>
#include "../Macros.hpp"

namespace Engine::Logging
{
	enum class LogLevel
	{
		Verbose,
		Debug,
		Info,
		Warning,
		Fatal
	};

	class Logger
	{
	private:
		Logger() = delete;
		EXPORT static void LogMessage(LogLevel level, const std::string_view& message);

		static LogLevel m_logOutputLevel;

	public:
		template <typename... Args>
		static void Log(LogLevel level, std::string_view rt_fmt_str, Args&&... args)
		{
			std::string formatted_string = std::vformat(rt_fmt_str, std::make_format_args(args...));
			LogMessage(level, formatted_string);
		}

		// Convenience functions

		template <typename... Args>
		static void Verbose(std::string_view rt_fmt_str, Args&&... args)
		{
			std::string formatted_string = std::vformat(rt_fmt_str, std::make_format_args(args...));
			LogMessage(LogLevel::Verbose, formatted_string);
		}

		template <typename... Args>
		static void Debug(std::string_view rt_fmt_str, Args&&... args)
		{
			std::string formatted_string = std::vformat(rt_fmt_str, std::make_format_args(args...));
			LogMessage(LogLevel::Debug, formatted_string);
		}

		template <typename... Args>
		static void Info(std::string_view rt_fmt_str, Args&&... args)
		{
			std::string formatted_string = std::vformat(rt_fmt_str, std::make_format_args(args...));
			LogMessage(LogLevel::Info, formatted_string);
		}

		template <typename... Args>
		static void Warning(std::string_view rt_fmt_str, Args&&... args)
		{
			std::string formatted_string = std::vformat(rt_fmt_str, std::make_format_args(args...));
			LogMessage(LogLevel::Warning, formatted_string);
		}

		template <typename... Args>
		static void Error(std::string_view rt_fmt_str, Args&&... args)
		{
			std::string formatted_string = std::vformat(rt_fmt_str, std::make_format_args(args...));
			LogMessage(LogLevel::Fatal, formatted_string);
		}

		EXPORT static void SetLogOutputLevel(LogLevel level);
	};
}