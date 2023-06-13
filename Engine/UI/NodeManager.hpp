#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <imgui_node_editor.h>

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

	class NodeManager
	{
	public:
		NodeManager();
		~NodeManager();
		void Draw();

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
			std::string Name;
			PinType Type;
			PinKind Kind;

			Pin(uint32_t id, const char* name, PinType type)
				: ID(id)
				, Node(nullptr)
				, Name(name)
				, Type(type)
				, Kind(PinKind::Input)
			{
			}
		};

		struct Node
		{
			uint32_t ID;
			std::string Name;
			std::vector<Pin> Inputs;
			std::vector<Pin> Outputs;
			ImColor Color;
			NodeType Type;

			Node(uint32_t id, const char* name, ImColor color = ImColor(100, 100, 100))
				: ID(id)
				, Name(name)
				, Color(color)
				, Type(NodeType::Blueprint)
			{
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

		void DrawIcon(const ImVec2& size, bool filled, ImU32 color, ImU32 innerColor);
		bool IsPinLinked(uint32_t id);
		Node* SpawnBranchNode();
		void BuildNode(Node* node);
		ImColor GetIconColor(PinType type);
		void DrawPinIcon(const Pin& pin, bool connected, uint8_t alpha);

		uint32_t m_currentId;
		const float m_pinIconSize = 24.0f;
		std::vector<Node> m_nodes;
		std::vector<Link> m_links;
	};
}