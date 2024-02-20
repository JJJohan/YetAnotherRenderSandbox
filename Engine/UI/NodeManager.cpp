#define IMGUI_DEFINE_MATH_OPERATORS
#include "NodeBuilder.hpp"
#include "NodeManager.hpp"
#include <imgui_node_editor.h>
#include <imgui_internal.h>

namespace Engine::UI
{
	static ax::NodeEditor::EditorContext* m_editor = nullptr;

	NodeManager::NodeManager()
		: m_currentId(0)
		, m_nodeMap()
		, m_links()
		, m_builder(std::make_unique<NodeBuilder>())
	{
		m_editor = ax::NodeEditor::CreateEditor();

		ax::NodeEditor::SetCurrentEditor(m_editor);
	}

	NodeManager::~NodeManager()
	{
		if (m_editor)
		{
			ax::NodeEditor::DestroyEditor(m_editor);
			m_editor = nullptr;
		}
	}

	void NodeManager::DrawPinIcon(const Pin& pin, bool connected, uint8_t alpha, const Colour& colour)
	{
		ImColor color = ImColor(colour);
		color.Value.w = alpha / 255.0f;

		DrawIcon(glm::vec2(24.0f, 24.0f), connected, color, ImColor(colour.R / 8, colour.G / 8, colour.B / 8, alpha));
	};

	bool NodeManager::Begin(const char* label)
	{
		m_nodeMap.clear();
		m_links.clear();

		m_currentId = 0;
		ax::NodeEditor::SetCurrentEditor(m_editor);
		ax::NodeEditor::Begin(label);

		return true;
	}

	void NodeManager::ZoomToContent()
	{
		ax::NodeEditor::NavigateToContent(0.0f);
	}

	void NodeManager::End()
	{
		for (auto& link : m_links)
			ax::NodeEditor::Link(link.ID, link.StartPinID, link.EndPinID, ImColor(link.Colour), 2.0f);

		ax::NodeEditor::End();
	}

	NodeManager::Node& NodeManager::GetOrCreateNode(const std::string& nodeName)
	{
		const auto& search = m_nodeMap.find(nodeName);
		if (search != m_nodeMap.cend())
		{
			return search->second;
		}

		auto& node = m_nodeMap[nodeName] = Node();
		node.Setup(m_currentId++);
		return node;
	}

	NodeManager::Pin& NodeManager::GetOrCreatePin(std::unordered_map<std::string, Pin>& pinMap, const std::string& pinName)
	{
		const auto& search = pinMap.find(pinName);
		if (search != pinMap.cend())
		{
			return search->second;
		}

		auto& pin = pinMap[pinName] = Pin();
		pin.Setup(m_currentId++);
		return pin;
	}

	void NodeManager::SetupLink(const char* outputNodeName, const char* outputPinName, const char* inputNodeName, 
		const char* inputPinName, const Colour& colour)
	{
		Node& outputNode = GetOrCreateNode(outputNodeName);
		Node& inputNode = GetOrCreateNode(inputNodeName);

		Pin& startPin = GetOrCreatePin(outputNode.Outputs, outputPinName);
		Pin& endPin = GetOrCreatePin(inputNode.Inputs, inputPinName);
		startPin.Node = &inputNode;
		endPin.Node = &outputNode;

		m_links.emplace_back(Link(m_currentId++, startPin.ID, endPin.ID, colour));
	}

	glm::vec2 NodeManager::GetNodeSize(const char* label) const
	{
		const auto& search = m_nodeMap.find(label);
		if (search != m_nodeMap.cend())
		{
			const ImVec2& size = ax::NodeEditor::GetNodeSize(search->second.ID);
			return glm::vec2(size.x, size.y);
		}

		return glm::vec2();
	}

	void NodeManager::DrawNode(const char* label, const glm::vec2& pos, const std::vector<NodePin>& inputs,
		const std::vector<NodePin>& outputs, const Colour& colour)
	{
		Node& node = GetOrCreateNode(label);

		ax::NodeEditor::SetNodePosition(node.ID, ImVec2(pos.x, pos.y));
		m_builder->Begin(node.ID);

		const ImColor nodeColor(colour);
		m_builder->Header(nodeColor);
		ImGui::Spring(0);
		ImGui::TextUnformatted(label);
		ImGui::Spring(1);
		ImGui::Dummy(ImVec2(0, 28));
		ImGui::Spring(0);
		m_builder->EndHeader();

		for (auto& inputPin : inputs)
		{
			Pin& input = GetOrCreatePin(node.Inputs, inputPin.Name);
			float alpha = ImGui::GetStyle().Alpha;
			m_builder->Input(input.ID);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
			DrawPinIcon(input, input.Node != nullptr, static_cast<uint8_t>(alpha * 255.0f), inputPin.Colour);
			ImGui::Spring(0);
			ImGui::TextUnformatted(inputPin.Name.c_str());
			ImGui::Spring(0);
			ImGui::PopStyleVar();
			m_builder->EndInput();
		}

		for (auto& outputPin : outputs)
		{
			Pin& output = GetOrCreatePin(node.Outputs, outputPin.Name);
			float alpha = ImGui::GetStyle().Alpha;
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
			m_builder->Output(output.ID);
			ImGui::Spring(0);
			ImGui::TextUnformatted(outputPin.Name.c_str());
			ImGui::Spring(0);
			DrawPinIcon(output, output.Node != nullptr, static_cast<uint8_t>(alpha * 255.0f), outputPin.Colour);
			ImGui::PopStyleVar();
			m_builder->EndOutput();
		}

		m_builder->End();
	}

	void NodeManager::DrawIcon(const glm::vec2& size, bool filled, uint32_t color, uint32_t innerColor)
	{
		const ImVec2& a = ImGui::GetCursorScreenPos();
		const ImVec2& b = a + ImVec2(size.x, size.y);
		if (!ImGui::IsRectVisible(a, b))
			return;

		ImRect rect = ImRect(a, b);
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		float rect_x = rect.Min.x;
		float rect_y = rect.Min.y;
		float rect_w = rect.Max.x - rect.Min.x;
		float rect_h = rect.Max.y - rect.Min.y;
		float rect_center_x = (rect.Min.x + rect.Max.x) * 0.5f;
		float rect_center_y = (rect.Min.y + rect.Max.y) * 0.5f;
		ImVec2 rect_center = ImVec2(rect_center_x, rect_center_y);
		const float outline_scale = rect_w / 24.0f;
		const int32_t extra_segments = static_cast<int32_t>(2 * outline_scale); // for full circle

		const float origin_scale = rect_w / 24.0f;

		const float offset_x = 1.0f * origin_scale;
		const float offset_y = 0.0f * origin_scale;
		const float margin = (filled ? 2.0f : 2.0f) * origin_scale;
		const float rounding = 0.1f * origin_scale;
		const float tip_round = 0.7f; // percentage of triangle edge (for tip)
		const ImRect canvas = ImRect(
			rect.Min.x + margin + offset_x,
			rect.Min.y + margin + offset_y,
			rect.Max.x - margin + offset_x,
			rect.Max.y - margin + offset_y);
		const float canvas_x = canvas.Min.x;
		const float canvas_y = canvas.Min.y;
		const float canvas_w = canvas.Max.x - canvas.Min.x;
		const float canvas_h = canvas.Max.y - canvas.Min.y;

		const float left = canvas_x + canvas_w * 0.5f * 0.3f;
		const float right = canvas_x + canvas_w - canvas_w * 0.5f * 0.3f;
		const float top = canvas_y + canvas_h * 0.5f * 0.2f;
		const float bottom = canvas_y + canvas_h - canvas_h * 0.5f * 0.2f;
		const float center_y = (top + bottom) * 0.5f;

		const ImVec2 tip_top = ImVec2(canvas_x + canvas_w * 0.5f, top);
		const ImVec2 tip_right = ImVec2(right, center_y);
		const ImVec2 tip_bottom = ImVec2(canvas_x + canvas_w * 0.5f, bottom);

		drawList->PathLineTo(ImVec2(left, top) + ImVec2(0, rounding));
		drawList->PathBezierCubicCurveTo(
			ImVec2(left, top),
			ImVec2(left, top),
			ImVec2(left, top) + ImVec2(rounding, 0));
		drawList->PathLineTo(tip_top);
		drawList->PathLineTo(tip_top + (tip_right - tip_top) * tip_round);
		drawList->PathBezierCubicCurveTo(
			tip_right,
			tip_right,
			tip_bottom + (tip_right - tip_bottom) * tip_round);
		drawList->PathLineTo(tip_bottom);
		drawList->PathLineTo(ImVec2(left, bottom) + ImVec2(rounding, 0));
		drawList->PathBezierCubicCurveTo(
			ImVec2(left, bottom),
			ImVec2(left, bottom),
			ImVec2(left, bottom) - ImVec2(0, rounding));

		if (!filled)
		{
			if (innerColor & 0xFF000000)
				drawList->AddConvexPolyFilled(drawList->_Path.Data, drawList->_Path.Size, innerColor);

			drawList->PathStroke(color, true, 2.0f * outline_scale);
		}
		else
			drawList->PathFillConvex(color);

		ImGui::Dummy(ImVec2(size.x, size.y));
	}
}