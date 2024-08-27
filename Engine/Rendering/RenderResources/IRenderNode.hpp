#pragma once

#include <unordered_map>
#include <string>
#include "RenderPassResourceInfo.hpp"

namespace Engine::Rendering
{
	class IBuffer;
	class IRenderImage;
	class Renderer;

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

		virtual bool Build(const Renderer& renderer,
			const std::unordered_map<std::string, IRenderImage*>& imageInputs,
			const std::unordered_map<std::string, IRenderImage*>& imageOutputs,
			const std::unordered_map<std::string, IBuffer*>& bufferInputs,
			const std::unordered_map<std::string, IBuffer*>& bufferOutputs)
		{
			for (const auto& pair : imageInputs)
			{
				auto iter = m_imageInputInfos.find(pair.first);
				if (iter != m_imageInputInfos.end())
					iter->second.Image = pair.second;
			}

			for (const auto& pair : imageOutputs)
			{
				auto iter = m_imageOutputInfos.find(pair.first);
				if (iter != m_imageOutputInfos.end())
					iter->second.Image = pair.second;
			}

			for (const auto& pair : bufferInputs)
			{
				auto iter = m_bufferInputInfos.find(pair.first);
				if (iter != m_bufferInputInfos.end())
					iter->second.Buffer = pair.second;
			}

			for (const auto& pair : bufferOutputs)
			{
				auto iter = m_bufferOutputInfos.find(pair.first);
				if (iter != m_bufferOutputInfos.end())
					iter->second.Buffer = pair.second;
			}

			return true;
		}

		virtual void UpdatePlaceholderFormats(Format swapchainFormat, Format depthFormat)
		{
		}

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