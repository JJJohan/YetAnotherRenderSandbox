#pragma once
#include <vector>
#include <memory>
#include <unordered_map>
#include <string>
#include <glm/glm.hpp>
#include "Core/Colour.hpp"

namespace Engine::UI
{
	class NodeBuilder;

	struct NodePin
	{
		std::string Name;
		Colour Colour;

		NodePin(std::string_view name, const Engine::Colour& colour = {1.0f, 1.0f, 1.0f})
			: Name(name)
			, Colour(colour)
		{
		}
	};

	class NodeManager
	{
	public:
		NodeManager();
		~NodeManager();

		bool Begin(const char* label);
		void SetupLink(const char* outputNodeName, const char* outputPinName, const char* inputNodeName,
			const char* inputPinName, const Colour& colour = {});
		void DrawNode(const char* label, const glm::vec2& pos, const std::vector<NodePin>& inputs,
			const std::vector<NodePin>& outputs, const Colour& colour = {});
		glm::vec2 GetNodeSize(const char* label) const;
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
			PinKind Kind;

			inline void Setup(uint32_t id)
			{
				ID = id;
				Node = nullptr;
				Kind = PinKind::Input;
			}
		};

		struct Node
		{
			uint32_t ID;
			std::unordered_map<std::string, Pin> Inputs;
			std::unordered_map<std::string, Pin> Outputs;
			Colour Colour;

			inline void Setup(uint32_t id, Engine::Colour colour = { 0.4f, 0.4f, 0.4f })
			{
				ID = id;
				Colour = colour;
			}
		};

		struct Link
		{
			uint32_t ID;
			uint32_t StartPinID;
			uint32_t EndPinID;
			Colour Colour;

			Link(uint32_t id, uint32_t startPinId, uint32_t endPinId, Engine::Colour colour = { 1.0f, 1.0f, 1.0f })
				: ID(id)
				, StartPinID(startPinId)
				, EndPinID(endPinId)
				, Colour(colour)
			{
			}
		};

		Node& GetOrCreateNode(const std::string& nodeName);
		Pin& GetOrCreatePin(std::unordered_map<std::string, Pin>& pinMap, const std::string& pinName);

		void DrawIcon(const glm::vec2& size, bool filled, uint32_t color, uint32_t innerColor);
		void DrawPinIcon(const Pin& pin, bool connected, uint8_t alpha, const Colour& colour);

		uint32_t m_currentId;
		std::unique_ptr<NodeBuilder> m_builder;
		std::unordered_map<std::string, Node> m_nodeMap;
		std::vector<Link> m_links;
	};
}