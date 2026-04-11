#pragma once
#include "PosixTime.h"
#include "PosixSocketTransport.h"

namespace lumalink::platform
{
    // Platform default UTC wall-clock.
    // On POSIX: uses clock_gettime(CLOCK_REALTIME).
    // On Posix: uses GetSystemTimeAsFileTime.
    using UtcClock = lumalink::platform::posix::PosixUtcClock;

    // Platform default monotonic clock.
    // On POSIX: uses clock_gettime(CLOCK_MONOTONIC).
    // On Posix: uses QueryPerformanceCounter.
    using MonotonicClock = lumalink::platform::posix::PosixMonotonicClock;

    // Platform default combined clock (monotonic + UTC).
    using Clock = lumalink::platform::posix::PosixClock;

    using TransportFactory = TransportFactoryWrapper<lumalink::platform::posix::PosixSocketTransportFactory>;
}