#pragma once

#include "Core/Macros.hpp"
#include "Core/Colour.hpp"
#include <vector>
#include "ScrollingGraphBuffer.hpp"
#include "NodeManager.hpp"
#include <memory>
#include <unordered_map>

namespace Engine
{
	struct ProgressInfo;
}

namespace Engine::UI
{
	class NodeManager;

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
		EXPORT bool CollapsingHeader(const char* label, bool startOpen) const;

		EXPORT void PlotGraphs(const char* label, const std::unordered_map<std::string, ScrollingGraphBuffer>& buffers,
			const glm::vec2& size = glm::vec2(-1, 0)) const;

		EXPORT bool BeginNodeEditor(const char* label) const;
		EXPORT void NodeSetupLink(const char* outputNodeName, const char* outputPinName, const char* inputNodeName,
			const char* inputPinName, const Colour& colour = {1.0f, 1.0f, 1.0f}) const;
		EXPORT void DrawNode(const char* label, const glm::vec2& pos, const std::vector<NodePin>& inputs, 
			const std::vector<NodePin>& outputs, const Colour& colour = {0.5f, 0.5f, 0.5f}) const;
		EXPORT void EndNodeEditor() const;
		EXPORT glm::vec2 GetNodeSize(const char* label) const;
		EXPORT void NodeEditorZoomToContent() const;

		EXPORT glm::vec2 GetContentRegionAvailable() const;
		EXPORT void SetCursorPos(const glm::vec2& pos) const;
		EXPORT void BeginDisabled(bool disabled = true) const;
		EXPORT void EndDisabled() const;

	private:
		std::unique_ptr<NodeManager> m_nodeManager;
	};
}