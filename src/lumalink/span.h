#pragma once

#include <cstddef>

#if (defined(__cplusplus) && __cplusplus >= 202002L) ||                         \
	(defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
#if defined(__has_include)
#if __has_include(<span>)
#include <span>

namespace lumalink {
	using std::dynamic_extent;
	using std::span;
}

#define LUMALINK_USING_STD_SPAN
#endif
#endif
#endif

#ifndef LUMALINK_USING_STD_SPAN
#include "tcb_span.hpp"

namespace lumalink {
	using ::tcb::dynamic_extent;
	using ::tcb::span;
}
#endif