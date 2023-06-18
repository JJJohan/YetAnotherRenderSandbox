#pragma once

#include "../Types.hpp"
#include "Core/Colour.hpp"

namespace Engine::Rendering
{
    class IImageView;

    enum class AttachmentLoadOp
    {
        Load,
        Clear,
        DontCare,
        None
    };
    
    enum class AttachmentStoreOp
    {
        Store,
        DontCare,
        None
    };

    struct ClearValue
    {
        union
        {
            Engine::Colour Colour;
            float Depth;
        };

        ClearValue(float depth)
        {
            Depth = depth;
        }

        ClearValue(Engine::Colour colour)
        {
            Colour = colour;
        }
    };

	struct AttachmentInfo
	{
        const IImageView* imageView;
        ImageLayout imageLayout;
        AttachmentLoadOp loadOp;
        AttachmentStoreOp storeOp;
        ClearValue clearValue;

        AttachmentInfo()
            : imageView(nullptr)
            , imageLayout(ImageLayout::Undefined)
            , loadOp(AttachmentLoadOp::DontCare)
            , storeOp(AttachmentStoreOp::DontCare)
            , clearValue(0.0f)
        {
        }

        AttachmentInfo(const IImageView* imageView, ImageLayout imageLayout, AttachmentLoadOp loadOp, AttachmentStoreOp storeOp, ClearValue clearValue = ClearValue(0.0f))
            : imageView(imageView)
            , imageLayout(imageLayout)
            , loadOp(loadOp)
            , storeOp(storeOp)
            , clearValue(clearValue)
        {
		}
	};
}