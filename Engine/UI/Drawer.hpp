#pragma once

#include "Core/Macros.hpp"
#include "Core/Colour.hpp"
#include <vector>

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
		EXPORT void End() const;

		EXPORT void Text(const char* fmt, ...) const;
		EXPORT bool Colour3(const char* label, Colour& clour) const;
		EXPORT bool Colour4(const char* label, Colour& clour) const;
		EXPORT bool SliderFloat(const char* label, float* value, float min, float max) const;
		EXPORT bool SliderInt(const char* label, int32_t* value, int32_t min, int32_t max) const;
		EXPORT bool Checkbox(const char* label, bool* value) const;
		EXPORT bool ComboBox(const char* label, const std::vector<const char*>& entries, int32_t* index) const;
		EXPORT void Progress(const ProgressInfo& progress) const;

		EXPORT bool BeginTabBar(const char* label) const;
		EXPORT void EndTabBar() const;
		EXPORT bool BeginTabItem(const char* label) const;
		EXPORT void EndTabItem() const;
		EXPORT bool CollapsingHeader(const char* label) const;

		EXPORT void BeginDisabled(bool disabled = true) const;
		EXPORT void EndDisabled() const;
	};
}