#pragma once

#include "Core/Macros.hpp"
#include "Core/Colour.hpp"

namespace Engine
{
	struct ProgressInfo;
}

namespace Engine::UI
{
	class Drawer
	{
	public:
		Drawer();
		EXPORT bool Begin(const char* label, bool* open = nullptr) const;
		EXPORT void Text(const char* fmt, ...) const;
		EXPORT bool Colour3(const char* label, Colour& clour) const;
		EXPORT bool Colour4(const char* label, Colour& clour) const;
		EXPORT bool SliderFloat(const char* label, float* value, float min, float max) const;
		EXPORT bool SliderInt(const char* label, int32_t* value, int32_t min, int32_t max) const;
		EXPORT bool Checkbox(const char* label, bool* value) const;
		EXPORT void End() const;
		EXPORT void Progress(const ProgressInfo& progress) const;
	};
}