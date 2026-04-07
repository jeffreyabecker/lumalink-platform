/* Provide a small platform-owned alias for span based on std::span when available.
   This replaces the previous dependency on the quarantined imported copy. */

#include <span>

namespace tcb {
	template <typename T, std::size_t N = std::dynamic_extent>
	using span = std::span<T, N>;
	constexpr std::size_t dynamic_extent = std::dynamic_extent;
}

namespace lumalink::platform::util {
	using tcb::span;
	using tcb::dynamic_extent;
}

