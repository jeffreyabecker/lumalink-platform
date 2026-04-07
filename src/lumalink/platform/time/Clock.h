#pragma once

#include "ClockTypes.h"

namespace lumalink::platform::time
{
    // Interface for a monotonic clock.
    // Returns always-increasing relative milliseconds suitable for measuring
    // elapsed time and scheduling.  Never compare these values against wall-clock
    // time; the epoch is implementation-defined (usually process/system start).
    class IMonotonicClock
    {
    public:
        virtual ~IMonotonicClock() = default;

        // Returns current monotonic time in milliseconds.
        virtual MonotonicMillis monotonicNow() const = 0;
    };

    // Interface for a UTC wall-clock.
    // Returns the current UTC time as a Unix timestamp.
    // Returns an empty optional when the UTC time is not yet known
    // (e.g., before an external synchronizer has provided a reference time).
    class IUtcClock
    {
    public:
        virtual ~IUtcClock() = default;

        // Returns current UTC time, or std::nullopt if UTC is not yet available.
        // UTC and GMT refer to the same time scale; UTC is used in the API.
        virtual OptionalUnixTime utcNow() const = 0;
    };

    // Combined clock interface for clocks that provide both monotonic and UTC time.
    class IClock : public IMonotonicClock, public IUtcClock
    {
    public:
        ~IClock() override = default;
    };

    // Interface for a settable UTC clock — used to inject or clear UTC time.
    // Implementations are useful for testing and for embedded scenarios
    // where an external source (NTP, GPS, RTC) provides the reference time.
    class ISettableUtcClock : public IUtcClock
    {
    public:
        ~ISettableUtcClock() override = default;

        // Set the UTC time.
        virtual void setUtcTime(const UnixTime& time) = 0;

        // Clear the UTC time so that utcNow() returns std::nullopt.
        virtual void clearUtcTime() = 0;
    };
}
