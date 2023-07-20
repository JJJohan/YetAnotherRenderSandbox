#pragma once

#include <vector>
#include <unordered_map>
#include "IRenderNode.hpp"

namespace Engine::Rendering
{
	class IBuffer;
	class IRenderImage;
	class Renderer;

	class IRenderResource : public IRenderNode
	{
	public:
		virtual ~IRenderResource() = default;

		virtual bool Build(const Renderer& renderer) = 0;

		inline const std::unordered_map<const char*, IBuffer*>& GetBufferOutputs() const { return m_bufferOutputs; }

		inline const std::unordered_map<const char*, IRenderImage*>& GetImageOutputs() const { return m_imageOutputs; }

		inline IBuffer* GetOutputBuffer(const char* name) const { return m_bufferOutputs.at(name); }

		inline IRenderImage* GetOutputImage(const char* name) const { return m_imageOutputs.at(name); }

		inline virtual size_t GetMemoryUsage() const { return 0; }

		inline virtual RenderNodeType GetNodeType() const override { return RenderNodeType::Resource; }

	protected:
		IRenderResource(const char* name)
			: IRenderNode(name)
			, m_bufferOutputs()
			, m_imageOutputs()
		{
		}

		std::unordered_map<const char*, IBuffer*> m_bufferOutputs;
		std::unordered_map<const char*, IRenderImage*> m_imageOutputs;
	};
}