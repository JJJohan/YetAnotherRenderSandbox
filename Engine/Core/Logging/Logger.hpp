#pragma once

#include <format>
#include "../Macros.hpp"

namespace Engine::Logging
{
	enum class LogLevel
	{
		VERBOSE,
		DEBUG,
		INFO,
		WARNING,
		FATAL
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
			LogMessage(LogLevel::VERBOSE, formatted_string);
		}

		template <typename... Args>
		static void Debug(std::string_view rt_fmt_str, Args&&... args)
		{
			std::string formatted_string = std::vformat(rt_fmt_str, std::make_format_args(args...));
			LogMessage(LogLevel::DEBUG, formatted_string);
		}

		template <typename... Args>
		static void Info(std::string_view rt_fmt_str, Args&&... args)
		{
			std::string formatted_string = std::vformat(rt_fmt_str, std::make_format_args(args...));
			LogMessage(LogLevel::INFO, formatted_string);
		}

		template <typename... Args>
		static void Warning(std::string_view rt_fmt_str, Args&&... args)
		{
			std::string formatted_string = std::vformat(rt_fmt_str, std::make_format_args(args...));
			LogMessage(LogLevel::WARNING, formatted_string);
		}

		template <typename... Args>
		static void Error(std::string_view rt_fmt_str, Args&&... args)
		{
			std::string formatted_string = std::vformat(rt_fmt_str, std::make_format_args(args...));
			LogMessage(LogLevel::FATAL, formatted_string);
		}

		EXPORT static void SetLogOutputLevel(LogLevel level);
	};
}