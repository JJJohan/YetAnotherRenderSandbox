#pragma once

#include <unordered_map>
#include <string>
#include <glm/glm.hpp>
#include "../Types.hpp"

namespace Engine::Rendering
{
	class IBuffer;
	class IRenderImage;
	class Renderer;

	enum RenderNodeType
	{
		Pass,
		Compute,
		Resource
	};

	struct RenderPassImageInfo
	{
		Format Format;
		bool IsRead;
		glm::uvec3 Dimensions;
		IRenderImage* Image;

		RenderPassImageInfo(Engine::Rendering::Format format = Format::Undefined, bool isRead = false, const glm::uvec3& dimensions = {}, IRenderImage* image = nullptr)
			: Format(format)
			, IsRead(isRead)
			, Dimensions(dimensions)
			, Image(image)
		{
		}
	};

	class IRenderNode
	{
	public:
		virtual ~IRenderNode() = default;

		inline const std::string& GetName() const { return m_name; }

		virtual bool BuildResources(const Renderer& renderer) { return true; }

		virtual void UpdateConnections(const Renderer& renderer, const std::vector<std::string_view>& passNames) {}

		inline virtual void ClearResources() {};

		inline void SetEnabled(bool enabled) { m_enabled = enabled; }

		inline bool GetEnabled() const { return m_enabled; }

		inline RenderNodeType GetNodeType() const { return m_nodeType; }

		inline const std::unordered_map<std::string, IBuffer*>& GetBufferInputs() const { return m_bufferInputs; }

		inline const std::unordered_map<std::string, IBuffer*>& GetBufferOutputs() const { return m_bufferOutputs; }

		inline const std::unordered_map<std::string, RenderPassImageInfo>& GetImageInputInfos() const { return m_imageInputInfos; }

		inline const std::unordered_map<std::string, RenderPassImageInfo>& GetImageOutputInfos() const { return m_imageOutputInfos; }

	protected:
		IRenderNode(std::string_view name, RenderNodeType nodeType)
			: m_name(name)
			, m_bufferInputs()
			, m_bufferOutputs()
			, m_imageInputInfos()
			, m_imageOutputInfos()
			, m_enabled(true)
			, m_nodeType(nodeType)
		{
		}

		std::unordered_map<std::string, IBuffer*> m_bufferInputs;
		std::unordered_map<std::string, IBuffer*> m_bufferOutputs;
		std::unordered_map<std::string, RenderPassImageInfo> m_imageInputInfos;
		std::unordered_map<std::string, RenderPassImageInfo> m_imageOutputInfos;

	private:
		std::string m_name;
		bool m_enabled;
		RenderNodeType m_nodeType;
	};
}