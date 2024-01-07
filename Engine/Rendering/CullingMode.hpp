#pragma once

namespace Engine::Rendering
{
	enum class CullingMode
	{
		Paused,
		Reset,
		Frustum,
		FrustumAndOcclusion
	};
}