#pragma once

#include "../Types.hpp"
#include "Core/Colour.hpp"

namespace Engine::Rendering
{
    class IRenderImage;

    enum class AttachmentLoadOp
    {
        Load,
        Clear,
        DontCare
    };
    
    enum class AttachmentStoreOp
    {
        Store,
        DontCare
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
        IRenderImage* renderImage;
        ImageLayout imageLayout;
        AttachmentLoadOp loadOp;
        AttachmentStoreOp storeOp;
        ClearValue clearValue;

        AttachmentInfo(IRenderImage* renderImage = nullptr, 
            ImageLayout imageLayout = ImageLayout::Undefined,
            AttachmentLoadOp loadOp = AttachmentLoadOp::DontCare, 
            AttachmentStoreOp storeOp = AttachmentStoreOp::Store,
            ClearValue clearValue = ClearValue(0.0f))
            : renderImage(renderImage)
            , imageLayout(imageLayout)
            , loadOp(loadOp)
            , storeOp(storeOp)
            , clearValue(clearValue)
        {
		}
	};
}