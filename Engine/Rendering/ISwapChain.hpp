#pragma once

#include "Types.hpp"
#include <optional>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "Resources/IImageView.hpp"
#include "Resources/IRenderImage.hpp"

namespace Engine::Rendering
{
	class ISwapChain
	{
	public:
		ISwapChain()
			: m_swapChainImageFormat(Format::Undefined)
			, m_swapChainExtent()
			, m_swapChainImages()
			, m_hdrSupport()
		{}

		virtual ~ISwapChain() = default;

		inline const Format& GetFormat() const { return m_swapChainImageFormat; }
		inline const glm::uvec2& GetExtent() const { return m_swapChainExtent; }

		inline IRenderImage& GetSwapChainImage(uint32_t imageIndex) { return *m_swapChainImages[imageIndex]; }
		inline bool IsHDRCapable() const { return m_hdrSupport.has_value() && m_hdrSupport.value(); }

	protected:
		Format m_swapChainImageFormat;
		glm::uvec2 m_swapChainExtent;
		std::vector<std::unique_ptr<IRenderImage>> m_swapChainImages;
		std::optional<bool> m_hdrSupport;
	};
}