#pragma once

#include <string_view>

namespace Engine
{
    class Utilities
    {
    public:
        static bool EqualsIgnoreCase(std::string_view lhs, std::string_view rhs);
    };
}