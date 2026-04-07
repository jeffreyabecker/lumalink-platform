/* Inline copy of imported span.hpp to make platform-owned API surface stable */

// Include the imported implementation and expose a stable alias under lumalink::platform::util
#include "../../../imported/httpadv/v1/util/span.hpp"

namespace lumalink::platform::util {
	using tcb::span;
	using tcb::dynamic_extent;
}

