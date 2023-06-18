#pragma once
#include <vector>
#include <memory>
#include <unordered_map>
#include "Core/Colour.hpp"
#include <imgui.h>

namespace Engine::UI
{
	enum class PinType
	{
		Flow,
		Bool,
		Int,
		Float,
		String,
		Object,
		Function,
		Delegate,
	};

	enum class NodeType
	{
		Blueprint,
		Simple,
		Tree,
		Comment,
		Houdini
	};

	class NodeBuilder;

	class NodeManager
	{
	public:
		NodeManager();
		~NodeManager();

		bool Begin(const char* label);
		void SetupLink(const char* outputNodeName, const char* outputPinName, const char* inputNodeName, const char* inputPinName);
		void DrawNode(const char* label, const ImVec2& pos, const std::vector<const char*>& inputs, 
			const std::vector<const char*>& outputs, const Colour& colour);
		void ZoomToContent();
		void End();

	private:
		enum class PinKind
		{
			Output,
			Input
		};

		struct Node;

		struct Pin
		{
			uint32_t ID;
			Node* Node;
			PinType Type;
			PinKind Kind;

			inline void Setup(uint32_t id, PinType type)
			{
				ID = id;
				Node = nullptr;
				Type = type;
				Kind = PinKind::Input;
			}
		};

		struct Node
		{
			uint32_t ID;
			std::unordered_map<const char*, Pin> Inputs;
			std::unordered_map<const char*, Pin> Outputs;
			ImColor Color;
			NodeType Type;

			inline void Setup(uint32_t id, ImColor color = ImColor(100, 100, 100))
			{
				ID = id;
				Color = color;
				Type = NodeType::Blueprint;
			}
		};

		struct Link
		{
			uint32_t ID;
			uint32_t StartPinID;
			uint32_t EndPinID;
			ImColor Color;

			Link(uint32_t id, uint32_t startPinId, uint32_t endPinId)
				: ID(id)
				, StartPinID(startPinId)
				, EndPinID(endPinId)
				, Color(255, 255, 255)
			{
			}
		};

		Node& GetOrCreateNode(const char* nodeName);
		Pin& GetOrCreatePin(std::unordered_map<const char*, Pin>& pinMap, const char* pinName);

		void DrawIcon(const ImVec2& size, bool filled, ImU32 color, ImU32 innerColor);
		ImColor GetIconColor(PinType type);
		void DrawPinIcon(const Pin& pin, bool connected, uint8_t alpha);

		uint32_t m_currentId;
		std::unique_ptr<NodeBuilder> m_builder;
		std::unordered_map<const char*, Node> m_nodeMap;
		std::vector<Link> m_links;
	};
}