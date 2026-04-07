#pragma once

#include "Clock.h"
#include "ClockTypes.h"

namespace lumalink::platform::time
{
    // In-memory clock useful for unit tests and embedded scenarios that need
    // manual control over both monotonic ticks and UTC wall-clock time.
    //
    // Monotonic behavior:
    //   - Starts at zero; advance it explicitly via advanceMillis().
    //   - monotonicNow() always returns the current accumulated value.
    //
    // UTC behavior:
    //   - Starts with no UTC time (utcNow() returns std::nullopt).
    //   - Call setUtcTime() to inject a UTC reference.
    //   - advanceMillis() also advances the UTC time by the same amount (if set),
    //     so a single call keeps both clocks in sync.
    //   - Call clearUtcTime() to reset the UTC time to "unknown".
    class ManualClock : public IClock, public ISettableUtcClock
    {
    public:
        ManualClock() = default;

        // ── IMonotonicClock ────────────────────────────────────────────────────

        MonotonicMillis monotonicNow() const override
        {
            return monotonic_;
        }

        // ── IUtcClock / ISettableUtcClock ─────────────────────────────────────

        OptionalUnixTime utcNow() const override
        {
            return utc_;
        }

        void setUtcTime(const UnixTime& time) override
        {
            utc_ = time;
        }

        void clearUtcTime() override
        {
            utc_ = std::nullopt;
        }

        // ── Manual control ─────────────────────────────────────────────────────

        // Advance both the monotonic clock and (if set) the UTC clock by delta.
        void advanceMillis(std::uint64_t deltaMillis)
        {
            monotonic_.value += deltaMillis;
            if (utc_.has_value())
            {
                const std::uint64_t totalMs =
                    static_cast<std::uint64_t>(utc_->subsecondMillis) + deltaMillis;
                utc_->seconds         += static_cast<std::int64_t>(totalMs / 1000U);
                utc_->subsecondMillis  = static_cast<std::uint16_t>(totalMs % 1000U);
            }
        }

        // Set the monotonic clock to an explicit value (e.g., for test setup).
        void setMonotonicMillis(std::uint64_t value)
        {
            monotonic_.value = value;
        }

    private:
        MonotonicMillis  monotonic_{};
        OptionalUnixTime utc_{};
    };
}
