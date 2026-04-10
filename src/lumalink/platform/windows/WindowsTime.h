#pragma once

#include "lumalink/time/Clock.h"
#include "lumalink/time/ClockTypes.h"

#include <windows.h>

namespace lumalink::platform::windows
{
    // Windows UTC wall-clock using GetSystemTimeAsFileTime.
    // This API returns the current UTC/GMT system time as a FILETIME, which is
    // a 64-bit count of 100-nanosecond intervals since 1601-01-01T00:00:00Z.
    // We convert to a Unix timestamp (seconds since 1970-01-01T00:00:00Z).
    class WindowsUtcClock : public lumalink::platform::time::IUtcClock
    {
    public:
        lumalink::platform::time::OptionalUnixTime utcNow() const override
        {
            FILETIME ft{};
            GetSystemTimeAsFileTime(&ft);

            // Combine the two 32-bit halves into a single 64-bit value.
            ULARGE_INTEGER uli{};
            uli.LowPart  = ft.dwLowDateTime;
            uli.HighPart = ft.dwHighDateTime;

            // FILETIME epoch is 1601-01-01; Unix epoch is 1970-01-01.
            // This is the offset in 100-nanosecond intervals from the FILETIME
            // epoch (1601) to the Unix epoch (1970).
            static constexpr std::uint64_t kFileTimeToUnixEpochOffset100ns = 116'444'736'000'000'000ULL;

            if (uli.QuadPart < kFileTimeToUnixEpochOffset100ns)
            {
                // Clock is set before the Unix epoch; return nullopt to signal
                // that UTC time is not reliably available.
                return std::nullopt;
            }

            const std::uint64_t intervals = uli.QuadPart - kFileTimeToUnixEpochOffset100ns;
            const std::uint64_t totalMs   = intervals / 10'000ULL;

            lumalink::platform::time::UnixTime result;
            result.seconds         = static_cast<std::int64_t>(totalMs / 1'000ULL);
            result.subsecondMillis = static_cast<std::uint16_t>(totalMs % 1'000ULL);
            return result;
        }
    };

    // Windows monotonic clock using QueryPerformanceCounter.
    // QPC is a high-resolution, non-decreasing counter that is not affected
    // by wall-clock adjustments.
    class WindowsMonotonicClock : public lumalink::platform::time::IMonotonicClock
    {
    public:
        WindowsMonotonicClock()
        {
            LARGE_INTEGER freq{};
            if (QueryPerformanceFrequency(&freq) && freq.QuadPart > 0)
            {
                frequencyHz_ = static_cast<std::uint64_t>(freq.QuadPart);
            }
        }

        lumalink::platform::time::MonotonicMillis monotonicNow() const override
        {
            if (frequencyHz_ == 0)
            {
                return {};
            }

            LARGE_INTEGER counter{};
            QueryPerformanceCounter(&counter);
            const std::uint64_t ms =
                static_cast<std::uint64_t>(counter.QuadPart) * 1'000ULL / frequencyHz_;
            return {ms};
        }

    private:
        std::uint64_t frequencyHz_ = 0;
    };

    // Combined Windows clock — provides both monotonic and UTC time.
    class WindowsClock : public lumalink::platform::time::IClock
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
        WindowsMonotonicClock monotonic_{};
        WindowsUtcClock       utc_{};
    };
}
