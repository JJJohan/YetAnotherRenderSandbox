#pragma once

#include "IRenderNode.hpp"

namespace Engine::Rendering
{
	class Renderer;

	class IRenderResource : public IRenderNode
	{
	public:
		virtual ~IRenderResource() = default;

		virtual bool Build(const Renderer& renderer) = 0;

		inline virtual size_t GetMemoryUsage() const { return 0; }

	protected:
		IRenderResource(const char* name)
			: IRenderNode(name, RenderNodeType::Resource)
		{
		}
	};
}