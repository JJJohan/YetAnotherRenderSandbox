#pragma once

#include "Core/Macros.hpp"
#include <string>

namespace Engine::Rendering
{
	enum class ShaderProgramType
	{
		Vertex,
		Fragment
	};

	class Shader
	{
	public:
		Shader();
		EXPORT virtual bool IsValid() const;
		EXPORT const std::string& GetName() const;

	protected:
		std::string GetProgramTypeName(ShaderProgramType type) const;
		std::string m_name;
	};
}