#pragma once

#include "Core/Macros.hpp"

namespace Engine::Rendering
{
	enum class NvidiaReflexMode
	{
		Off,
		On,
		OnPlusBoost
	};

	enum class NvidiaReflexMarker
	{
		SimulationStart = 0,
		SimulationEnd = 1,
		RenderSubmitStart = 2,
		RenderSubmitEnd = 3,
		PresentStart = 4,
		PresentEnd = 5,
		InputSample = 6,
		TriggerFlash = 7,
		OutOfBandRenderSubmitStart = 8,
		OutOfBandRenderSubmitEnd = 9,
		OutOfBandPresentStart = 10,
		OutOfBandPresentEnd = 11
	};

	class NvidiaReflex
	{
	public:
		NvidiaReflex();
		virtual ~NvidiaReflex();
		EXPORT virtual bool SetMode(NvidiaReflexMode mode) = 0;
		virtual bool Sleep() const = 0;
		virtual void SetMarker(NvidiaReflexMarker marker) const = 0;
		inline NvidiaReflexMode GetMode() const { return m_mode; }
		inline bool IsSupported() const { return m_supported; }

	protected:
		bool m_supported;
		NvidiaReflexMode m_mode;
	};
}