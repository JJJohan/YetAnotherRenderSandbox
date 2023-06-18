#include "Material.hpp"
#include "OS/Files.hpp"
#include "Core/Logging/Logger.hpp"
#include "Core/Utilities.hpp"
#include <rapidjson/document.h>

using namespace Engine::OS;
using namespace Engine::Logging;

namespace Engine::Rendering
{
	Material::Material()
		: m_name()
		, m_programData()
		, m_attachmentFormats()
		, m_depthTest(false)
		, m_depthWrite(false)
	{
	}

	inline bool TryGetMember(std::string_view name, rapidjson::Value& node, const char* elementName, rapidjson::Value& member)
	{
		if (!node.HasMember(elementName))
		{
			Logger::Error("Material '{}' is missing required element '{}'.", name, elementName);
			return false;
		}

		member = node[elementName];
		return true;
	}

	inline bool TryGetBool(std::string_view name, rapidjson::Value& node, const char* elementName, bool& value)
	{
		rapidjson::Value member;
		if (!TryGetMember(name, node, elementName, member))
			return false;

		if (!member.IsBool())
		{
			Logger::Error("Material '{}' element '{}' is not a boolean type.", name, elementName);
			return false;
		}

		value = member.GetBool();
		return true;
	}

	inline bool TryGetStringValue(std::string_view name, rapidjson::Value& node, const char* elementName, std::string& value)
	{
		rapidjson::Value member;
		if (!TryGetMember(name, node, elementName, member))
			return false;

		if (!member.IsString())
		{
			Logger::Error("Material '{}' element '{}' is not a string type.", name, elementName);
			return false;
		}

		value = std::string(member.GetString());
		return true;
	}

	inline void LogInvalidProgram(std::string_view name, rapidjson::Document& document, const char* elementName)
	{
		Logger::Error("Material '{}' has invalid data type for element: {}", name, elementName);
	}

	inline bool TryParseProgramType(std::string_view name, std::string& string, ShaderStageFlags& programType)
	{
		Utilities::StringToLower(string);
		if (string == "vertex")
		{
			programType = ShaderStageFlags::Vertex;
			return true;
		}
		else if (string == "fragment")
		{
			programType = ShaderStageFlags::Fragment;
			return true;
		}

		Logger::Error("Material '{}' contained unexpected value for program type: {}", name, string);
		return false;
	}

	inline bool TryParseAttachmentFormat(std::string_view name, std::string& string, Format& attachmentFormat)
	{
		Utilities::StringToLower(string);

		if (string == "r8g8unorm")
		{
			attachmentFormat = Format::R8G8Unorm;
			return true;
		}
		else if (string == "r8g8b8a8unorm")
		{
			attachmentFormat = Format::R8G8B8A8Unorm;
			return true;
		}
		else if (string == "r16g16sfloat")
		{
			attachmentFormat = Format::R16G16Sfloat;
			return true;
		}
		else if (string == "r16g16b16a16sfloat")
		{
			attachmentFormat = Format::R16G16B16A16Sfloat;
			return true;
		}
		else if (string == "r32g32b32a32sfloat")
		{
			attachmentFormat = Format::R32G32B32A32Sfloat;
			return true;
		}
		else if (string == "swapchain")
		{
			attachmentFormat = Format::Swapchain;
			return true;
		}

		Logger::Error("Material '{}' contained unexpected value for attachmnet format: {}", name, string);
		return false;
	}

	bool Material::Parse(const std::filesystem::path& path)
	{
		m_name = path.stem().string();

		std::vector<char> json;
		if (!Files::TryReadTextFile(path.string(), json))
		{
			Logger::Error("Failed to read material file: {}", path.string().c_str());
			return false;
		}

		rapidjson::Document document;
		document.Parse(json.data());
		if (document.HasParseError())
		{
			rapidjson::ParseErrorCode errorCode = document.GetParseError();
			Logger::Error("Error occured parsing material '{}'. Error code: {}", m_name, static_cast<uint32_t>(errorCode));
			return false;
		}

		rapidjson::Value programs;
		if (!TryGetMember(m_name, document, "Programs", programs))
		{
			return false;
		}

		if (!programs.IsArray())
		{
			Logger::Error("Material '{}' has invalid data type for element 'Program'.", m_name);
			return false;
		}

		for (auto& program : programs.GetArray())
		{
			std::string typeString;
			if (!TryGetStringValue(m_name, program, "Type", typeString))
				return false;

			std::string pathString;
			if (!TryGetStringValue(m_name, program, "Path", pathString))
				return false;

			ShaderStageFlags programType;
			if (!TryParseProgramType(m_name, typeString, programType))
				return false;

			if (m_programData.contains(programType))
			{
				Logger::Error("Material '{}' contains duplicate program types: {}", m_name, typeString);
				return false;
			}

			std::vector<uint8_t> programData;
			if (!Files::TryReadBinaryFile(pathString, programData))
			{
				Logger::Error("Could not read program at path '{}' defined in material '{}'.", pathString, m_name);
				return false;
			}

			m_programData[programType] = programData;
		}

		if (!TryGetBool(m_name, document, "DepthWrite", m_depthWrite))
		{
			return false;
		}

		if (!TryGetBool(m_name, document, "DepthTest", m_depthTest))
		{
			return false;
		}

		rapidjson::Value attachments;
		if (!TryGetMember(m_name, document, "Attachments", attachments))
		{
			return false;
		}

		if (!attachments.IsArray())
		{
			Logger::Error("Material '{}' has invalid data type for element 'Attachments'.", m_name);
			return false;
		}

		for (auto& attachment : attachments.GetArray())
		{
			if (!attachment.IsString())
			{
				Logger::Error("Material '{}' has invalid data type for element 'Attachments'.", m_name);
				return false;
			}

			std::string attachmentString = std::string(attachment.GetString());
			Format attachmentFormat;
			if (!TryParseAttachmentFormat(m_name, attachmentString, attachmentFormat))
				return false;

			m_attachmentFormats.push_back(attachmentFormat);
		}

		return true;
	}
}