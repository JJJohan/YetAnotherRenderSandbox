#pragma once

namespace Engine::Rendering
{
	class IImageMemoryBarriers
	{
	public:
		virtual bool Empty() const = 0;
		virtual void Clear() = 0;

	protected:
		IImageMemoryBarriers()
		{
		}
	};
}