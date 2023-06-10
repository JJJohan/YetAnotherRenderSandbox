#include "Core/Macros.hpp"
#include <vector>
#include <string>

namespace Engine::OS
{
	class Files
	{
	public:
		EXPORT static bool TryReadBinaryFile(const std::string& filePath, std::vector<uint8_t>& memory);
		EXPORT static bool TryReadTextFile(const std::string& filePath, std::vector<char>& memory);
	};
}