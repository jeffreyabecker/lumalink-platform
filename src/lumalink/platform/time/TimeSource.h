#pragma once

#include "ClockTypes.h"
#include "Clock.h"

namespace lumalink::platform::time
{
    // Result returned by an IUtcTimeSource when queried for the current UTC time.
    // ok == false indicates that the source could not provide a time (e.g., no
    // network connectivity, hardware not ready, or not yet synchronised).
    struct UtcTimeResult
    {
        bool    ok   = false;
        UnixTime time{};

        // Convenience: implicit bool test.
        explicit constexpr operator bool() const { return ok; }
    };

    // Minimal interface for an external UTC time source.
    // Implementations may query NTP, GPS, RTC, or any other authoritative source.
    // The interface deliberately does not expose transport details so that the
    // base clock layer remains independent of networking.
    //
    // To add NTP later:
    //   class NtpTimeSource : public IUtcTimeSource { ... };
    class IUtcTimeSource
    {
    public:
        virtual ~IUtcTimeSource() = default;

        // Query the source for the current UTC time.
        // Returns a UtcTimeResult; check result.ok before using result.time.
        virtual UtcTimeResult fetchUtcTime() = 0;
    };

    // Interface for a component that synchronises a settable UTC clock from a
    // time source.  Decouple the "how do we get time" (IUtcTimeSource) from
    // the "how do we apply time" (ISettableUtcClock) so either half can be
    // swapped independently.
    //
    // Typical flow:
    //   1. Construct with a source and a target clock.
    //   2. Call synchronize() periodically (or once at startup).
    //   3. The target clock's utcNow() will return the synchronised time.
    class IUtcSynchronizer
    {
    public:
        virtual ~IUtcSynchronizer() = default;

        // Perform a synchronisation attempt.
        // Returns the UtcTimeResult from the underlying source; the caller can
        // inspect result.ok to know whether the clock was updated.
        virtual UtcTimeResult synchronize() = 0;
    };

    // Concrete synchronizer that wires an IUtcTimeSource to an ISettableUtcClock.
    // Keeps no additional state beyond its two constructor arguments.
    class UtcSynchronizer : public IUtcSynchronizer
    {
    public:
        // Both pointers must outlive this object.
        UtcSynchronizer(IUtcTimeSource& source, ISettableUtcClock& clock)
            : source_(source), clock_(clock)
        {
        }

        UtcTimeResult synchronize() override
        {
            const UtcTimeResult result = source_.fetchUtcTime();
            if (result.ok)
            {
                clock_.setUtcTime(result.time);
            }
            return result;
        }

    private:
        IUtcTimeSource&    source_;
        ISettableUtcClock& clock_;
    };
}
