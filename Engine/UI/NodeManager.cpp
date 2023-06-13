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
		, m_pinIconSize(24.0f)
		, m_nodes()
		, m_links()
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

	bool NodeManager::IsPinLinked(uint32_t id)
	{
		if (!id)
			return false;

		for (auto& link : m_links)
			if (link.StartPinID == id || link.EndPinID == id)
				return true;

		return false;
	}

	NodeManager::Node* NodeManager::SpawnBranchNode()
	{
		m_nodes.emplace_back(++m_currentId, "Branch");
		m_nodes.back().Inputs.emplace_back(++m_currentId, "Test", PinType::Flow);
		m_nodes.back().Outputs.emplace_back(++m_currentId, "Albedo", PinType::Flow);
		m_nodes.back().Outputs.emplace_back(++m_currentId, "WorldNormal", PinType::Flow);
		m_nodes.back().Outputs.emplace_back(++m_currentId, "WorldPos", PinType::Flow);
		m_nodes.back().Outputs.emplace_back(++m_currentId, "MetalRoughness", PinType::Flow);
		m_nodes.back().Outputs.emplace_back(++m_currentId, "Velocity", PinType::Flow);

		BuildNode(&m_nodes.back());

		return &m_nodes.back();
	}

	void NodeManager::BuildNode(Node* node)
	{
		for (auto& input : node->Inputs)
		{
			input.Node = node;
			input.Kind = PinKind::Input;
		}

		for (auto& output : node->Outputs)
		{
			output.Node = node;
			output.Kind = PinKind::Output;
		}
	}

	ImColor NodeManager::GetIconColor(PinType type)
	{
		switch (type)
		{
		default:
		case PinType::Flow:     return ImColor(255, 255, 255);
		case PinType::Bool:     return ImColor(220, 48, 48);
		case PinType::Int:      return ImColor(68, 201, 156);
		case PinType::Float:    return ImColor(147, 226, 74);
		case PinType::String:   return ImColor(124, 21, 153);
		case PinType::Object:   return ImColor(51, 150, 215);
		case PinType::Function: return ImColor(218, 0, 183);
		case PinType::Delegate: return ImColor(255, 48, 48);
		}
	};

	void NodeManager::DrawPinIcon(const Pin& pin, bool connected, uint8_t alpha)
	{
		ImColor  color = GetIconColor(pin.Type);
		color.Value.w = alpha / 255.0f;

		DrawIcon(ImVec2(m_pinIconSize, m_pinIconSize), connected, color, ImColor(32, 32, 32, alpha));
	};

	void NodeManager::Draw()
	{
		auto& io = ImGui::GetIO();

		ax::NodeEditor::SetCurrentEditor(m_editor);

		ax::NodeEditor::Begin("Node editor");
		{
			NodeBuilder builder;

			for (auto& node : m_nodes)
			{
				builder.Begin(node.ID);

				builder.Header(node.Color);
				ImGui::Spring(0);
				ImGui::TextUnformatted(node.Name.c_str());
				ImGui::Spring(1);
				ImGui::Dummy(ImVec2(0, 28));
				ImGui::Spring(0);
				builder.EndHeader();

				for (auto& input : node.Inputs)
				{
					float alpha = ImGui::GetStyle().Alpha;
					builder.Input(input.ID);
					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
					DrawPinIcon(input, IsPinLinked(input.ID), static_cast<uint8_t>(alpha * 255.0f));
					ImGui::Spring(0);
					if (!input.Name.empty())
					{
						ImGui::TextUnformatted(input.Name.c_str());
						ImGui::Spring(0);
					}
					ImGui::PopStyleVar();
					builder.EndInput();
				}

				for (auto& output : node.Outputs)
				{
					float alpha = ImGui::GetStyle().Alpha;
					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
					builder.Output(output.ID);
					if (!output.Name.empty())
					{
						ImGui::Spring(0);
						ImGui::TextUnformatted(output.Name.c_str());
					}
					ImGui::Spring(0);
					DrawPinIcon(output, IsPinLinked(output.ID), static_cast<uint8_t>(alpha * 255.0f));
					ImGui::PopStyleVar();
					builder.EndOutput();
				}

				builder.End();
			}

			for (auto& link : m_links)
				ax::NodeEditor::Link(link.ID, link.StartPinID, link.EndPinID, link.Color, 2.0f);
		}

		ax::NodeEditor::End();
	}

	void NodeManager::DrawIcon(const ImVec2& size, bool filled, ImU32 color, ImU32 innerColor)
	{
		if (ImGui::IsRectVisible(size))
			return;

		const ImVec2& a = ImGui::GetCursorScreenPos();
		const ImVec2& b = a + size;

		ImDrawList* drawList = ImGui::GetWindowDrawList();

		ImRect rect = ImRect(a, b);
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

		ImGui::Dummy(size);
	}
}