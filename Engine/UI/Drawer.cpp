#include "Drawer.hpp"
#include "Core/AsyncData.hpp"
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

	void Drawer::Progress(const ProgressInfo& progress) const
	{
		ImGuiWindowFlags flags;
		flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar;

		const char* progressText = progress.ProgressText.c_str();
		const char* subProgressText = progress.SubProgressText.c_str();

		bool hasSubProgress = subProgressText != nullptr;
		ImVec2 dialogSize(300.0f, hasSubProgress ? 120.0f : 80.0f);
		const ImVec2& center = ImGui::GetIO().DisplaySize;
		const ImGuiStyle& style = ImGui::GetStyle();
		float progressWidth = dialogSize.x - style.WindowPadding.x * 2.0f;

		ImGui::SetNextWindowPos(ImVec2(center.x * 0.5f - dialogSize.x * 0.5f, center.y * 0.5f - dialogSize.y * 0.5f));
		ImGui::SetNextWindowSize(dialogSize);
		if (ImGui::Begin("Progress", nullptr, flags))
		{
			float textWidth = ImGui::CalcTextSize(progressText).x;
			ImGui::SetCursorPosX((dialogSize.x - textWidth) * 0.5f);
			ImGui::Text(progressText);

			ImGui::SetNextItemWidth(progressWidth);
			ImGui::ProgressBar(progress.Progress, ImVec2(0.0f, 0.0f));
			if (hasSubProgress)
			{
				float textWidth = ImGui::CalcTextSize(subProgressText).x;
				ImGui::SetCursorPosX((dialogSize.x - textWidth) * 0.5f);
				ImGui::Text(subProgressText);

				ImGui::SetNextItemWidth(progressWidth);
				ImGui::ProgressBar(progress.SubProgress, ImVec2(0.0f, 0.0f));
			}
			ImGui::End();
		}
	}
}