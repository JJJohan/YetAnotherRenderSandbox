#include "Shader.hpp"

namespace Engine::Rendering
{
	Shader::Shader()
		: m_name()
	{
	}

	bool Shader::IsValid() const
	{
		return false;
	}

	const std::string& Shader::GetName() const
	{
		return m_name;
	}

	std::string Shader::GetProgramTypeName(ShaderProgramType type) const
	{
		switch (type)
		{
		case ShaderProgramType::Vertex:
			return "Vertex";
		case ShaderProgramType::Fragment:
			return "Fragment";
		default:
			return "Unknown";
		}
	}
}