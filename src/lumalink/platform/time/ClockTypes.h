#pragma once

#include <cstdint>
#include <optional>

namespace lumalink::platform::time
{
    // Monotonic relative time in milliseconds (always increasing, no epoch meaning).
    // Use for measuring elapsed time and scheduling; never compare against wall-clock time.
    struct MonotonicMillis
    {
        std::uint64_t value = 0;

        constexpr bool operator==(const MonotonicMillis& other) const { return value == other.value; }
        constexpr bool operator!=(const MonotonicMillis& other) const { return value != other.value; }
        constexpr bool operator<(const MonotonicMillis& other) const { return value < other.value; }
        constexpr bool operator<=(const MonotonicMillis& other) const { return value <= other.value; }
        constexpr bool operator>(const MonotonicMillis& other) const { return value > other.value; }
        constexpr bool operator>=(const MonotonicMillis& other) const { return value >= other.value; }

        constexpr MonotonicMillis operator+(const MonotonicMillis& other) const { return {value + other.value}; }
        constexpr MonotonicMillis operator-(const MonotonicMillis& other) const { return {value - other.value}; }
    };

    // UTC wall-clock time as a Unix timestamp (seconds since 1970-01-01T00:00:00Z).
    // The subsecondMillis field carries sub-second precision (0–999 ms).
    // UTC and GMT refer to the same time scale; UTC is preferred in the API.
    struct UnixTime
    {
        std::int64_t  seconds         = 0;  // Unix epoch seconds (UTC)
        std::uint16_t subsecondMillis = 0;  // 0–999 ms additional precision

        constexpr bool operator==(const UnixTime& other) const
        {
            return seconds == other.seconds && subsecondMillis == other.subsecondMillis;
        }
        constexpr bool operator!=(const UnixTime& other) const { return !(*this == other); }
        constexpr bool operator<(const UnixTime& other) const
        {
            return seconds < other.seconds ||
                   (seconds == other.seconds && subsecondMillis < other.subsecondMillis);
        }
        constexpr bool operator<=(const UnixTime& other) const { return !(other < *this); }
        constexpr bool operator>(const UnixTime& other) const { return other < *this; }
        constexpr bool operator>=(const UnixTime& other) const { return !(*this < other); }
    };

    // Nullable UTC time result — used by clocks whose UTC time may not yet be known
    // (e.g., before an external synchronizer has run).
    using OptionalUnixTime = std::optional<UnixTime>;
}
