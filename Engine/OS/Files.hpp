#include "Core/Macros.hpp"
#include <vector>
#include <string>

namespace Engine::OS
{
	class Files
	{
	public:
		EXPORT static bool TryReadFile(const std::string& filePath, std::vector<uint8_t>& memory);
	};
}