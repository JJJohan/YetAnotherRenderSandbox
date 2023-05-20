#include "Logger.hpp"
#include <iostream>
#include <mutex>

#ifdef _MSC_VER
#define DEBUG_BREAK __debugbreak()
#else
#define DEBUG_BREAK (void)
#endif

namespace Engine::Logging
{
	LogLevel Logger::m_logOutputLevel = LogLevel::DEBUG;
	static std::mutex m_logMutex;

	void Logger::LogMessage(LogLevel level, const std::string_view& message)
	{
		if (level < Logger::m_logOutputLevel)
			return;


		const std::lock_guard<std::mutex> guard(m_logMutex);
		switch (level)
		{
		case LogLevel::VERBOSE:
			std::cout << "\033[35m" << "[VERBOSE] " << message << "\033[0m" << std::endl;
			break;
		case LogLevel::DEBUG:
			std::cout << "\033[36m" << "[DEBUG] " << message << "\033[0m" << std::endl;
			break;
		case LogLevel::INFO:
			std::cout << "\033[37m" << "[INFO] " << message << "\033[0m" << std::endl;
			break;
		case LogLevel::WARNING:
			std::cout << "\033[33m" << "[WARNING] " << message << "\033[0m" << std::endl;
			break;
		case LogLevel::FATAL:
			std::cout << "\033[31m" << "[ERROR] " << message << "\033[0m" << std::endl;
			DEBUG_BREAK;
			break;
		}
	}
	void Logger::SetLogOutputLevel(LogLevel level)
	{
		m_logOutputLevel = level;
	}
}