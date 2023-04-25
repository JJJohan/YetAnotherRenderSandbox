#pragma once

#include "Utils.hpp"
#include <atomic>

namespace Engine
{
    uint32_t Utils::GetId() 
    {
        static std::atomic<uint32_t> id {0};
        return ++id;
    }
}