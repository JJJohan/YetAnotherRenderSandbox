#pragma once

#include <unordered_map>
#include <string>
#include "RenderPassResourceInfo.hpp"

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

		inline const std::unordered_map<std::string, RenderPassBufferInfo>& GetBufferInputInfos() const { return m_bufferInputInfos; }

		inline const std::unordered_map<std::string, RenderPassBufferInfo>& GetBufferOutputInfos() const { return m_bufferOutputInfos; }

		inline const std::unordered_map<std::string, RenderPassImageInfo>& GetImageInputInfos() const { return m_imageInputInfos; }

		inline const std::unordered_map<std::string, RenderPassImageInfo>& GetImageOutputInfos() const { return m_imageOutputInfos; }

	protected:
		IRenderNode(std::string_view name, RenderNodeType nodeType)
			: m_name(name)
			, m_bufferInputInfos()
			, m_bufferOutputInfos()
			, m_imageInputInfos()
			, m_imageOutputInfos()
			, m_enabled(true)
			, m_nodeType(nodeType)
		{
		}

		std::unordered_map<std::string, RenderPassBufferInfo> m_bufferInputInfos;
		std::unordered_map<std::string, RenderPassBufferInfo> m_bufferOutputInfos;
		std::unordered_map<std::string, RenderPassImageInfo> m_imageInputInfos;
		std::unordered_map<std::string, RenderPassImageInfo> m_imageOutputInfos;

	private:
		std::string m_name;
		bool m_enabled;
		RenderNodeType m_nodeType;
	};
}