#pragma once

namespace Engine::Rendering
{
	class IDevice;

	class ISemaphore
	{
	public:
		ISemaphore()
			: Value(0)
		{
		}

		virtual ~ISemaphore() = default;

		virtual bool Initialise(const IDevice& device, bool binary = false) = 0;

		uint64_t Value;
	};
}