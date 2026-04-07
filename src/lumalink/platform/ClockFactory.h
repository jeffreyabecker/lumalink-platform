#pragma once

#include "time/ClockTypes.h"
#include "time/Clock.h"
#include "time/ManualClock.h"
#include "time/TimeSource.h"

#if defined(_WIN32)
#include "./windows/WindowsTime.h"
#else
#include "./posix/PosixTime.h"
#endif

namespace lumalink::platform
{
    namespace detail
    {
#if defined(_WIN32)
        using NativeUtcClock       = lumalink::platform::windows::WindowsUtcClock;
        using NativeMonotonicClock = lumalink::platform::windows::WindowsMonotonicClock;
        using NativeClock          = lumalink::platform::windows::WindowsClock;
#else
        using NativeUtcClock       = lumalink::platform::posix::PosixUtcClock;
        using NativeMonotonicClock = lumalink::platform::posix::PosixMonotonicClock;
        using NativeClock          = lumalink::platform::posix::PosixClock;
#endif
    }

    // Platform default UTC wall-clock.
    // On POSIX: uses clock_gettime(CLOCK_REALTIME).
    // On Windows: uses GetSystemTimeAsFileTime.
    using UtcClock = detail::NativeUtcClock;

    // Platform default monotonic clock.
    // On POSIX: uses clock_gettime(CLOCK_MONOTONIC).
    // On Windows: uses QueryPerformanceCounter.
    using MonotonicClock = detail::NativeMonotonicClock;

    // Platform default combined clock (monotonic + UTC).
    using Clock = detail::NativeClock;
}
