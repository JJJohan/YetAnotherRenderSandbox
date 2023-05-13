#include "Drawer.hpp"
#include <cstdarg>
#include <imgui.h>

namespace Engine::UI
{
	Drawer::Drawer()
	{
	}

	bool Drawer::Begin(const char* label, bool* open) const
	{
		return ImGui::Begin(label, open);
	}

	void Drawer::Text(const char* fmt, ...) const
	{
		va_list args;
		va_start(args, fmt);

		size_t len = vsnprintf(0, 0, fmt, args);
		char* buffer = new char[len + 1];
		vsnprintf(buffer, len + 1, fmt, args);
		ImGui::Text(buffer);

		va_end(args);
	}

	bool Drawer::Colour3(const char* label, Colour& colour) const
	{
		glm::vec4 rgba = colour.GetVec4();
		if (ImGui::ColorEdit3(label, &rgba[0]))
		{
			colour = Colour(rgba);
			return true;
		}

		return false;
	}	
	
	bool Drawer::Colour4(const char* label, Colour& colour) const
	{
		glm::vec4 rgba = colour.GetVec4();
		if (ImGui::ColorEdit4(label, &rgba[0]))
		{
			colour = Colour(rgba);
			return true;
		}

		return false;
	}

	bool Drawer::SliderFloat(const char* label, float* value, float min, float max) const
	{
		return ImGui::SliderFloat(label, value, min, max);
	}
	
	bool Drawer::SliderInt(const char* label, int32_t* value, int32_t min, int32_t max) const
	{
		return ImGui::SliderInt(label, value, min, max);
	}

	bool Drawer::Checkbox(const char* label, bool* value) const
	{
		return ImGui::Checkbox(label, value);
	}

	void Drawer::End() const
	{
		ImGui::End();
	}
}