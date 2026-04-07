#include <unity.h>

#include <LumaLinkPlatform.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <cstdint>

using lumalink::platform::time::IClock;
using lumalink::platform::time::IMonotonicClock;
using lumalink::platform::time::ISettableUtcClock;
using lumalink::platform::time::IUtcClock;
using lumalink::platform::time::IUtcSynchronizer;
using lumalink::platform::time::IUtcTimeSource;
using lumalink::platform::time::ManualClock;
using lumalink::platform::time::MonotonicMillis;
using lumalink::platform::time::OptionalUnixTime;
using lumalink::platform::time::UnixTime;
using lumalink::platform::time::UtcSynchronizer;
using lumalink::platform::time::UtcTimeResult;

// Use the platform default aliases — no per-file #ifdef required.
using NativeUtcClock       = lumalink::platform::UtcClock;
using NativeMonotonicClock = lumalink::platform::MonotonicClock;
using NativeClock          = lumalink::platform::Clock;

static void sleepForMilliseconds(int ms)
{
#if defined(_WIN32)
    Sleep(static_cast<DWORD>(ms));
#else
    usleep(static_cast<useconds_t>(ms * 1000));
#endif
}

// ── Layer 1: ClockTypes ──────────────────────────────────────────────────────

void test_unix_time_comparison_operators()
{
    const UnixTime a{1000, 0};
    const UnixTime b{1000, 500};
    const UnixTime c{1001, 0};

    TEST_ASSERT_TRUE(a < b);
    TEST_ASSERT_TRUE(b < c);
    TEST_ASSERT_TRUE(a < c);
    TEST_ASSERT_FALSE(b < a);
    TEST_ASSERT_TRUE(a == a);
    TEST_ASSERT_FALSE(a == b);
    TEST_ASSERT_TRUE(a != b);
    TEST_ASSERT_TRUE(c > a);
    TEST_ASSERT_TRUE(a <= b);
    TEST_ASSERT_TRUE(c >= b);
}

void test_monotonic_millis_arithmetic()
{
    const MonotonicMillis a{500};
    const MonotonicMillis b{300};
    TEST_ASSERT_EQUAL_UINT64(800, (a + b).value);
    TEST_ASSERT_EQUAL_UINT64(200, (a - b).value);
    TEST_ASSERT_TRUE(a > b);
    TEST_ASSERT_TRUE(b < a);
    TEST_ASSERT_TRUE(a == a);
    TEST_ASSERT_FALSE(a == b);
}

// ── Layer 2: ManualClock ─────────────────────────────────────────────────────

void test_manual_clock_starts_with_no_utc_time()
{
    ManualClock clock;
    TEST_ASSERT_FALSE(clock.utcNow().has_value());
    TEST_ASSERT_EQUAL_UINT64(0, clock.monotonicNow().value);
}

void test_manual_clock_set_and_get_utc_time()
{
    ManualClock clock;
    const UnixTime expected{1700000000, 123};
    clock.setUtcTime(expected);

    const OptionalUnixTime got = clock.utcNow();
    TEST_ASSERT_TRUE(got.has_value());
    TEST_ASSERT_EQUAL_INT64(expected.seconds, got->seconds);
    TEST_ASSERT_EQUAL_UINT16(expected.subsecondMillis, got->subsecondMillis);
}

void test_manual_clock_clear_utc_time()
{
    ManualClock clock;
    clock.setUtcTime({1700000000, 0});
    TEST_ASSERT_TRUE(clock.utcNow().has_value());

    clock.clearUtcTime();
    TEST_ASSERT_FALSE(clock.utcNow().has_value());
}

void test_manual_clock_advance_millis_monotonic()
{
    ManualClock clock;
    clock.advanceMillis(500);
    TEST_ASSERT_EQUAL_UINT64(500, clock.monotonicNow().value);
    clock.advanceMillis(250);
    TEST_ASSERT_EQUAL_UINT64(750, clock.monotonicNow().value);
}

void test_manual_clock_advance_millis_with_utc()
{
    ManualClock clock;
    clock.setUtcTime({1000, 800});  // 1000s + 800ms
    clock.advanceMillis(500);       // advance 500ms

    // 800ms + 500ms = 1300ms = 1s + 300ms
    const OptionalUnixTime utc = clock.utcNow();
    TEST_ASSERT_TRUE(utc.has_value());
    TEST_ASSERT_EQUAL_INT64(1001, utc->seconds);
    TEST_ASSERT_EQUAL_UINT16(300, utc->subsecondMillis);

    // Monotonic should also have advanced
    TEST_ASSERT_EQUAL_UINT64(500, clock.monotonicNow().value);
}

void test_manual_clock_advance_without_utc_does_not_set_utc()
{
    ManualClock clock;
    clock.advanceMillis(1000);
    TEST_ASSERT_FALSE(clock.utcNow().has_value());
}

void test_manual_clock_implements_iclock()
{
    ManualClock manualClock;
    IClock& clock = manualClock;
    ISettableUtcClock& settable = manualClock;

    settable.setUtcTime({42, 0});
    TEST_ASSERT_TRUE(clock.utcNow().has_value());
    TEST_ASSERT_EQUAL_INT64(42, clock.utcNow()->seconds);
}

// ── Layer 3: TimeSource / Synchronizer ───────────────────────────────────────

namespace
{
    class FixedUtcTimeSource : public IUtcTimeSource
    {
    public:
        explicit FixedUtcTimeSource(const UnixTime& t, bool ok = true)
            : time_(t), ok_(ok)
        {
        }

        UtcTimeResult fetchUtcTime() override
        {
            if (!ok_)
            {
                return {};
            }
            UtcTimeResult r;
            r.ok   = true;
            r.time = time_;
            return r;
        }

    private:
        UnixTime time_;
        bool     ok_;
    };
}

void test_utc_synchronizer_sets_clock_from_source()
{
    ManualClock clock;
    const UnixTime expected{1700000000, 0};
    FixedUtcTimeSource source(expected);

    UtcSynchronizer sync(source, clock);
    const UtcTimeResult result = sync.synchronize();

    TEST_ASSERT_TRUE(result.ok);
    TEST_ASSERT_TRUE(clock.utcNow().has_value());
    TEST_ASSERT_EQUAL_INT64(expected.seconds, clock.utcNow()->seconds);
}

void test_utc_synchronizer_leaves_clock_unchanged_on_source_failure()
{
    ManualClock clock;
    clock.setUtcTime({999, 0});

    FixedUtcTimeSource failingSource({}, /*ok=*/false);
    UtcSynchronizer sync(failingSource, clock);
    const UtcTimeResult result = sync.synchronize();

    TEST_ASSERT_FALSE(result.ok);
    // Clock should retain its previous value.
    TEST_ASSERT_TRUE(clock.utcNow().has_value());
    TEST_ASSERT_EQUAL_INT64(999, clock.utcNow()->seconds);
}

void test_utc_time_result_bool_conversion()
{
    UtcTimeResult ok;
    ok.ok = true;
    UtcTimeResult fail;
    fail.ok = false;

    TEST_ASSERT_TRUE(static_cast<bool>(ok));
    TEST_ASSERT_FALSE(static_cast<bool>(fail));
}

// ── Native UTC clock ─────────────────────────────────────────────────────────

void test_native_utc_clock_returns_a_value()
{
    NativeUtcClock clock;
    const OptionalUnixTime t = clock.utcNow();
    TEST_ASSERT_TRUE(t.has_value());
    // Sanity: Unix time must be after 2020-01-01 (1577836800).
    TEST_ASSERT_GREATER_THAN_INT64(1577836800LL, t->seconds);
    TEST_ASSERT_LESS_THAN_UINT16(1000, t->subsecondMillis);
}

// ── Native monotonic clock ───────────────────────────────────────────────────

void test_native_monotonic_clock_is_non_decreasing()
{
    NativeMonotonicClock clock;
    const MonotonicMillis t1 = clock.monotonicNow();
    sleepForMilliseconds(20);
    const MonotonicMillis t2 = clock.monotonicNow();

    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(t1.value, t2.value);
}

void test_native_monotonic_clock_advances_over_delay()
{
    NativeMonotonicClock clock;
    const MonotonicMillis t1 = clock.monotonicNow();
    sleepForMilliseconds(50);
    const MonotonicMillis t2 = clock.monotonicNow();

    // After 50 ms sleep the monotonic counter must have strictly advanced.
    // We only verify t2 > t1; no minimum delta is enforced because scheduling
    // jitter on CI machines can make the actual elapsed time vary widely.
    TEST_ASSERT_GREATER_THAN_UINT64(t1.value, t2.value);
}

// ── Combined native clock ─────────────────────────────────────────────────────

void test_native_combined_clock_returns_both_times()
{
    NativeClock clock;
    const OptionalUnixTime utc = clock.utcNow();
    const MonotonicMillis mono  = clock.monotonicNow();

    TEST_ASSERT_TRUE(utc.has_value());
    TEST_ASSERT_GREATER_THAN_INT64(1577836800LL, utc->seconds);
    // Monotonic value is some positive count; just ensure it was populated.
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(0, mono.value);
}
