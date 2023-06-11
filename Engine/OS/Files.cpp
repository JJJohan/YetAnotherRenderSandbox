#include "Files.hpp"
#include <fstream>

namespace Engine::OS
{
	bool Files::TryReadBinaryFile(const std::string& filePath, std::vector<uint8_t>& memory)
	{
		std::ifstream file(filePath, std::ios::ate | std::ios::binary);
		if (!file.is_open())
		{
			return false;
		}

		size_t fileSize = static_cast<size_t>(file.tellg());
		memory.resize(fileSize);
		file.seekg(0);
		file.read(reinterpret_cast<char*>(memory.data()), fileSize);
		file.close();

		return true;
	}

	bool Files::TryReadTextFile(const std::string& filePath, std::vector<char>& memory)
	{
		std::ifstream file(filePath, std::ios::ate);
		if (!file.is_open())
		{
			return false;
		}

		size_t fileSize = static_cast<size_t>(file.tellg());
		memory.resize(fileSize);
		file.seekg(0);
		file.read(static_cast<char*>(memory.data()), fileSize);
		file.close();

		return true;
	}

	bool Files::TryWriteBinaryFile(const std::string& filePath, const std::vector<uint8_t>& memory)
	{
		std::ofstream file(filePath, std::ios::trunc | std::ios::binary);
		if (!file.is_open())
		{
			return false;
		}

		file.write(reinterpret_cast<const char*>(memory.data()), memory.size());
		file.close();

		return true;
	}

	bool Files::TryWriteTextFile(const std::string& filePath, const std::vector<char>& memory)
	{
		std::ofstream file(filePath, std::ios::trunc);
		if (!file.is_open())
		{
			return false;
		}

		file.write(memory.data(), memory.size());
		file.close();

		return true;
	}
}