#pragma once

#include "IRenderNode.hpp"

namespace Engine::Rendering
{
	class IRenderResource : public IRenderNode
	{
	public:
		virtual ~IRenderResource() = default;

		inline virtual size_t GetMemoryUsage() const { return 0; }

	protected:
		IRenderResource(std::string_view name)
			: IRenderNode(name, RenderNodeType::Resource)
		{
		}
	};
}