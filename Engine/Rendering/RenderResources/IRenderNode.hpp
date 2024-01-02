#pragma once

#include <unordered_map>
#include <glm/glm.hpp>
#include "../Types.hpp"

namespace Engine::Rendering
{
	class IBuffer;
	class IRenderImage;

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

		inline const char* GetName() const { return m_name; }

		inline virtual void ClearResources() {};

		inline void SetEnabled(bool enabled) { m_enabled = enabled; }

		inline bool GetEnabled() const { return m_enabled; }

		inline RenderNodeType GetNodeType() const { return m_nodeType; }

		inline const std::unordered_map<const char*, IBuffer*>& GetBufferInputs() const { return m_bufferInputs; }

		inline const std::unordered_map<const char*, IBuffer*>& GetBufferOutputs() const { return m_bufferOutputs; }

		inline const std::unordered_map<const char*, RenderPassImageInfo>& GetImageInputInfos() const { return m_imageInputInfos; }

		inline const std::unordered_map<const char*, RenderPassImageInfo>& GetImageOutputInfos() const { return m_imageOutputInfos; }

	protected:
		IRenderNode(const char* name, RenderNodeType nodeType)
			: m_name(name)
			, m_bufferInputs()
			, m_bufferOutputs()
			, m_imageInputInfos()
			, m_imageOutputInfos()
			, m_enabled(true)
			, m_nodeType(nodeType)
		{
		}

		std::unordered_map<const char*, IBuffer*> m_bufferInputs;
		std::unordered_map<const char*, IBuffer*> m_bufferOutputs;
		std::unordered_map<const char*, RenderPassImageInfo> m_imageInputInfos;
		std::unordered_map<const char*, RenderPassImageInfo> m_imageOutputInfos;

	private:
		const char* m_name;
		bool m_enabled;
		RenderNodeType m_nodeType;
	};
}