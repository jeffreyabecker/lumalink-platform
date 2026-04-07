#pragma once

// Minimal platform-owned defines extracted from the imported copy.
// If additional values are needed, expand this header during audit.

#include <cstdint>

namespace lumalink::platform::core {
	constexpr int MAX_CONCURRENT_CONNECTIONS = 8;
}

// Back-compat alias for old imported namespace
namespace httpadv::v1::core {
	constexpr int MAX_CONCURRENT_CONNECTIONS = lumalink::platform::core::MAX_CONCURRENT_CONNECTIONS;
}
