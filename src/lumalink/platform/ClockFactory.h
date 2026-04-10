#pragma once

#include "lumalink/time/ClockTypes.h"
#include "lumalink/time/Clock.h"
#include "lumalink/time/ManualClock.h"
#include "lumalink/time/TimeSource.h"

#if defined(ARDUINO)
#include "./arduino/ArduinoTime.h"
#elif defined(_WIN32)
#include "./windows/WindowsTime.h"
#else
#include "./posix/PosixTime.h"
#endif

namespace lumalink::platform
{
    namespace detail
    {
#if defined(ARDUINO)
    using NativeUtcClock       = lumalink::platform::arduino::ArduinoUtcClock;
    using NativeMonotonicClock = lumalink::platform::arduino::ArduinoMonotonicClock;
    using NativeClock          = lumalink::platform::arduino::ArduinoClock;
#elif defined(_WIN32)
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
    // On Arduino: uses the core's system UTC time after it has been populated by
    // an external synchronizer such as Arduino-Pico's WiFi NTP client.
    // On POSIX: uses clock_gettime(CLOCK_REALTIME).
    // On Windows: uses GetSystemTimeAsFileTime.
    using UtcClock = detail::NativeUtcClock;

    // Platform default monotonic clock.
    // On Arduino: uses millis().
    // On POSIX: uses clock_gettime(CLOCK_MONOTONIC).
    // On Windows: uses QueryPerformanceCounter.
    using MonotonicClock = detail::NativeMonotonicClock;

    // Platform default combined clock (monotonic + UTC).
    using Clock = detail::NativeClock;
}
