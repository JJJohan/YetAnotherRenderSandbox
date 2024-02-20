#include "Material.hpp"
#include "OS/Files.hpp"
#include "Core/Logging/Logger.hpp"
#include "Core/Utilities.hpp"
#include <simdjson.h>

using namespace Engine::OS;
using namespace Engine::Logging;
using namespace simdjson;

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

	inline bool TryGetMember(std::string_view name, ondemand::object node, std::string_view elementName, ondemand::value& result)
	{
		if (node[elementName].get(result) != simdjson::SUCCESS)
		{
			Logger::Error("Material '{}' is missing required element '{}'.", name, elementName);
			return false;
		}

		return true;
	}

	inline bool TryGetArray(std::string_view name, ondemand::object node, std::string_view elementName, ondemand::array& value)
	{
		ondemand::value member;
		if (!TryGetMember(name, node, elementName, member))
			return false;

		if (member.type() != ondemand::json_type::array)
		{
			Logger::Error("Material '{}' element '{}' is not an array.", name, elementName);
			return false;
		}

		value = member.get_array();
		return true;
	}

	inline bool TryGetBool(std::string_view name, ondemand::object node, std::string_view elementName, bool& value)
	{
		ondemand::value member;
		if (!TryGetMember(name, node, elementName, member))
			return false;

		if (member.type() != ondemand::json_type::boolean)
		{
			Logger::Error("Material '{}' element '{}' is not a boolean type.", name, elementName);
			return false;
		}

		value = member.get_bool();
		return true;
	}

	inline bool TryGetStringValue(std::string_view name, ondemand::object node, std::string_view elementName, std::string& value)
	{
		ondemand::value member;
		if (!TryGetMember(name, node, elementName, member))
			return false;

		if (member.type() != ondemand::json_type::string)
		{
			Logger::Error("Material '{}' element '{}' is not a string type.", name, elementName);
			return false;
		}

		value = std::string(member.get_string().value());
		return true;
	}

	inline void LogInvalidProgram(std::string_view name, ondemand::object node, std::string_view elementName)
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
		else if (string == "compute")
		{
			programType = ShaderStageFlags::Compute;
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
			attachmentFormat = Format::PlaceholderSwapchain;
			return true;
		}

		Logger::Error("Material '{}' contained unexpected value for attachment format: {}", name, string);
		return false;
	}

	inline bool CheckError(std::string_view name, error_code errorCode)
	{
		if (errorCode)
		{
			Logger::Error("Error occurred parsing material '{}'. Error code: {}", name, static_cast<uint32_t>(errorCode));
			return false;
		}

		return true;
	}

	bool Material::Parse(const std::filesystem::path& path)
	{
		m_name = path.stem().string();

		ondemand::parser parser;
		padded_string json;
		if (!CheckError(m_name, padded_string::load(path.string()).get(json)))
			return false;

		ondemand::document documentNode;
		if (!CheckError(m_name, parser.iterate(json).get(documentNode)))
			return false;

		auto document = documentNode.get_object().value();

		ondemand::array programs;
		if (!TryGetArray(m_name, document, "Programs", programs))
		{
			return false;
		}

		bool isCompute = false;
		for (auto programNode : programs)
		{
			auto program = programNode.get_object().value();

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

			if (programType == ShaderStageFlags::Compute)
				isCompute = true;

			m_programData[programType] = programData;
		}

		if (!isCompute)
		{
			if (!TryGetBool(m_name, document, "DepthWrite", m_depthWrite))
			{
				return false;
			}

			if (!TryGetBool(m_name, document, "DepthTest", m_depthTest))
			{
				return false;
			}

			ondemand::array attachments;
			if (!TryGetArray(m_name, document, "Attachments", attachments))
			{
				return false;
			}

			for (auto attachment : attachments)
			{
				if (attachment.type() != ondemand::json_type::string)
				{
					Logger::Error("Material '{}' has invalid data type for element 'Attachments'.", m_name);
					return false;
				}

				std::string attachmentString = std::string(attachment.get_string().value());
				Format attachmentFormat;
				if (!TryParseAttachmentFormat(m_name, attachmentString, attachmentFormat))
					return false;

				m_attachmentFormats.push_back(attachmentFormat);
			}
		}

		return true;
	}
}