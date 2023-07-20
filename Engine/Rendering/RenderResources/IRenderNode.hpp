#pragma once

namespace Engine::Rendering
{
	enum RenderNodeType
	{
		Pass,
		Resource
	};

	class IRenderNode
	{
	public:
		virtual ~IRenderNode() = default;

		inline const char* GetName() const { return m_name; }

		inline virtual void ClearResources() {};

		inline virtual RenderNodeType GetNodeType() const = 0;

	protected:
		IRenderNode(const char* name)
			: m_name(name)
		{
		}

	private:
		const char* m_name;
	};
}