#pragma once

#include "lumalink/platform/util/span.hpp"

namespace lumalink
{
    namespace util = platform::util;
    template <typename T, std::size_t N = platform::util::dynamic_extent>
    using Span = platform::util::span<T, N>;
}
