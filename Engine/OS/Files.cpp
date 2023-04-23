#include "Files.hpp"
#include <fstream>

namespace Engine::OS
{
	bool Files::TryReadFile(const std::string& filePath, std::vector<uint8_t>& memory)
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
}