#pragma once

#include <cstdint>
#include <optional>

namespace lumalink::platform::time
{
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

    struct UnixTime
    {
        std::int64_t  seconds         = 0;
        std::uint16_t subsecondMillis = 0;

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

    using OptionalUnixTime = std::optional<UnixTime>;
}