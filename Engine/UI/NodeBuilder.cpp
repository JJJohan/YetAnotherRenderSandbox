//------------------------------------------------------------------------------
// LICENSE
//   This software is dual-licensed to the public domain and under the following
//   license: you are granted a perpetual, irrevocable license to copy, modify,
//   publish, and distribute this file as you see fit.
//
// CREDITS
//   Written by Michal Cichon
//------------------------------------------------------------------------------
#define IMGUI_DEFINE_MATH_OPERATORS
#include "NodeBuilder.hpp"

namespace Engine::UI
{
	NodeBuilder::NodeBuilder()
		: CurrentNodeId(0)
		, CurrentStage(Stage::Invalid)
		, HasHeader(false)
		, HeaderColor(0)
	{
	}

	void NodeBuilder::Begin(uint32_t id)
	{
		HasHeader = false;
		HeaderMin = HeaderMax = ImVec2();

		ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_NodePadding, ImVec4(8, 4, 8, 8));

		ax::NodeEditor::BeginNode(id);

		ImGui::PushID(id);
		CurrentNodeId = id;

		SetStage(Stage::Begin);
	}

	void NodeBuilder::End()
	{
		SetStage(Stage::End);

		ax::NodeEditor::EndNode();

		if (ImGui::IsItemVisible())
		{
			uint8_t alpha = static_cast<uint8_t>(255.0f * ImGui::GetStyle().Alpha);

			ImDrawList* drawList = ax::NodeEditor::GetNodeBackgroundDrawList(CurrentNodeId);

			const float halfBorderWidth = ax::NodeEditor::GetStyle().NodeBorderWidth * 0.5f;

			uint32_t headerColor = IM_COL32(0, 0, 0, alpha) | (HeaderColor & IM_COL32(255, 255, 255, 0));
			if ((HeaderMax.x > HeaderMin.x) && (HeaderMax.y > HeaderMin.y))
			{
				drawList->AddRectFilled(
					HeaderMin - ImVec2(8 - halfBorderWidth, 4 - halfBorderWidth),
					HeaderMax + ImVec2(8 - halfBorderWidth, 0),
					headerColor, ax::NodeEditor::GetStyle().NodeRounding, ImDrawFlags_RoundCornersTop);

				if (ContentMin.y > HeaderMax.y)
				{
					drawList->AddLine(
						ImVec2(HeaderMin.x - (8 - halfBorderWidth), HeaderMax.y - 0.5f),
						ImVec2(HeaderMax.x + (8 - halfBorderWidth), HeaderMax.y - 0.5f),
						ImColor(255, 255, 255, 96 * alpha / (3 * 255)), 1.0f);
				}
			}
		}

		CurrentNodeId = 0;

		ImGui::PopID();

		ax::NodeEditor::PopStyleVar();

		SetStage(Stage::Invalid);
	}

	void NodeBuilder::Header(const ImVec4& color)
	{
		HeaderColor = ImColor(color);
		SetStage(Stage::Header);
	}

	void NodeBuilder::EndHeader()
	{
		SetStage(Stage::Content);
	}

	void NodeBuilder::Input(uint32_t id)
	{
		if (CurrentStage == Stage::Begin)
			SetStage(Stage::Content);

		const auto applyPadding = (CurrentStage == Stage::Input);

		SetStage(Stage::Input);

		if (applyPadding)
			ImGui::Spring(0);

		Pin(id, ax::NodeEditor::PinKind::Input);

		ImGui::BeginHorizontal(id);
	}

	void NodeBuilder::EndInput()
	{
		ImGui::EndHorizontal();
		EndPin();
	}

	void NodeBuilder::Middle()
	{
		if (CurrentStage == Stage::Begin)
			SetStage(Stage::Content);

		SetStage(Stage::Middle);
	}

	void NodeBuilder::Output(uint32_t id)
	{
		if (CurrentStage == Stage::Begin)
			SetStage(Stage::Content);

		const auto applyPadding = (CurrentStage == Stage::Output);

		SetStage(Stage::Output);

		if (applyPadding)
			ImGui::Spring(0);

		Pin(id, ax::NodeEditor::PinKind::Output);

		ImGui::BeginHorizontal(id);
	}

	void NodeBuilder::EndOutput()
	{
		ImGui::EndHorizontal();
		EndPin();
	}

	bool NodeBuilder::SetStage(Stage stage)
	{
		if (stage == CurrentStage)
			return false;

		auto oldStage = CurrentStage;
		CurrentStage = stage;

		ImVec2 cursor;
		switch (oldStage)
		{
		case Stage::Begin:
			break;

		case Stage::Header:
			ImGui::EndHorizontal();
			HeaderMin = ImGui::GetItemRectMin();
			HeaderMax = ImGui::GetItemRectMax();

			// spacing between header and content
			ImGui::Spring(0, ImGui::GetStyle().ItemSpacing.y * 2.0f);

			break;

		case Stage::Content:
			break;

		case Stage::Input:
			ax::NodeEditor::PopStyleVar(2);
			ImGui::Spring(1, 0);
			ImGui::EndVertical();
			break;

		case Stage::Middle:
			ImGui::EndVertical();
			break;

		case Stage::Output:
			ax::NodeEditor::PopStyleVar(2);
			ImGui::Spring(1, 0);
			ImGui::EndVertical();
			break;

		case Stage::End:
			break;

		case Stage::Invalid:
			break;
		}

		switch (stage)
		{
		case Stage::Begin:
			ImGui::BeginVertical("node");
			break;

		case Stage::Header:
			HasHeader = true;

			ImGui::BeginHorizontal("header");
			break;

		case Stage::Content:
			if (oldStage == Stage::Begin)
				ImGui::Spring(0);

			ImGui::BeginHorizontal("content");
			ImGui::Spring(0, 0);
			break;

		case Stage::Input:
			ImGui::BeginVertical("inputs", ImVec2(0, 0), 0.0f);

			ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_PivotAlignment, ImVec2(0, 0.5f));
			ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_PivotSize, ImVec2(0, 0));

			if (!HasHeader)
				ImGui::Spring(1, 0);
			break;

		case Stage::Middle:
			ImGui::Spring(1);
			ImGui::BeginVertical("middle", ImVec2(0, 0), 1.0f);
			break;

		case Stage::Output:
			if (oldStage == Stage::Middle || oldStage == Stage::Input)
				ImGui::Spring(1);
			else
				ImGui::Spring(1, 0);
			ImGui::BeginVertical("outputs", ImVec2(0, 0), 1.0f);

			ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_PivotAlignment, ImVec2(1.0f, 0.5f));
			ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_PivotSize, ImVec2(0, 0));

			if (!HasHeader)
				ImGui::Spring(1, 0);
			break;

		case Stage::End:
			if (oldStage == Stage::Input)
				ImGui::Spring(1, 0);
			if (oldStage != Stage::Begin)
				ImGui::EndHorizontal();
			ContentMin = ImGui::GetItemRectMin();
			ContentMax = ImGui::GetItemRectMax();

			//ImGui::Spring(0);
			ImGui::EndVertical();
			NodeMin = ImGui::GetItemRectMin();
			NodeMax = ImGui::GetItemRectMax();
			break;

		case Stage::Invalid:
			break;
		}

		return true;
	}

	void NodeBuilder::Pin(uint32_t id, ax::NodeEditor::PinKind kind)
	{
		ax::NodeEditor::BeginPin(id, kind);
	}

	void NodeBuilder::EndPin()
	{
		ax::NodeEditor::EndPin();
	}
}