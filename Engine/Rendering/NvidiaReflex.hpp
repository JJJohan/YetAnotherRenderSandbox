#pragma once

#include "Core/Macros.hpp"

namespace Engine::Rendering
{
	enum class NvidiaReflexMode
	{
		Off,
		On,
		OnPlusBoost
	};

	class NvidiaReflex
	{
	public:
		NvidiaReflex();
		EXPORT virtual bool SetMode(NvidiaReflexMode mode) = 0;
		virtual bool Sleep() const = 0;
		inline NvidiaReflexMode GetMode() const { return m_mode; }
		inline bool IsSupported() const { return m_supported; }

	protected:
		bool m_supported;
		NvidiaReflexMode m_mode;
	};
}