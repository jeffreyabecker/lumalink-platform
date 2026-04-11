#pragma once

#include "../lumalink/platform/Clock.h"

#include <time.h>

namespace lumalink::platform::posix
{
    // POSIX UTC wall-clock using clock_gettime(CLOCK_REALTIME).
    // CLOCK_REALTIME tracks the system UTC/GMT time and may be adjusted by NTP
    // or the operator, but is the correct source for absolute wall-clock time.
    class PosixUtcClock : public lumalink::platform::time::IUtcClock
    {
    public:
        lumalink::platform::time::OptionalUnixTime utcNow() const override
        {
            struct timespec ts{};
            if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
            {
                return std::nullopt;
            }

            lumalink::platform::time::UnixTime result;
            result.seconds         = static_cast<std::int64_t>(ts.tv_sec);
            result.subsecondMillis = static_cast<std::uint16_t>(
                static_cast<std::uint32_t>(ts.tv_nsec) / 1'000'000U);
            return result;
        }
    };

    // POSIX monotonic clock using clock_gettime(CLOCK_MONOTONIC).
    // CLOCK_MONOTONIC is not affected by NTP slewing or system clock changes,
    // making it the correct source for measuring elapsed time.
    class PosixMonotonicClock : public lumalink::platform::time::IMonotonicClock
    {
    public:
        lumalink::platform::time::MonotonicMillis monotonicNow() const override
        {
            struct timespec ts{};
            if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
            {
                return {};
            }

            const std::uint64_t ms =
                static_cast<std::uint64_t>(ts.tv_sec) * 1'000ULL +
                static_cast<std::uint64_t>(ts.tv_nsec) / 1'000'000ULL;
            return {ms};
        }
    };

    // Combined POSIX clock — provides both monotonic and UTC time.
    class PosixClock : public lumalink::platform::time::IClock
    {
    public:
        lumalink::platform::time::MonotonicMillis monotonicNow() const override
        {
            return monotonic_.monotonicNow();
        }

        lumalink::platform::time::OptionalUnixTime utcNow() const override
        {
            return utc_.utcNow();
        }

    private:
        PosixMonotonicClock monotonic_{};
        PosixUtcClock       utc_{};
    };
}
