#pragma once
#include "WindowsTime.h"
#include "WindowsSocketTransport.h"

namespace lumalink::platform
{
    // Platform default UTC wall-clock.
    // On POSIX: uses clock_gettime(CLOCK_REALTIME).
    // On Windows: uses GetSystemTimeAsFileTime.
    using UtcClock = lumalink::platform::windows::WindowsUtcClock;

    // Platform default monotonic clock.
    // On POSIX: uses clock_gettime(CLOCK_MONOTONIC).
    // On Windows: uses QueryPerformanceCounter.
    using MonotonicClock = lumalink::platform::windows::WindowsMonotonicClock;

    // Platform default combined clock (monotonic + UTC).
    using Clock = lumalink::platform::windows::WindowsClock;

    using TransportFactory = TransportFactoryWrapper<lumalink::platform::windows::WindowsSocketTransportFactory>;
}