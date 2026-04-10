# C++23 Modernization Backlog

Purpose: identify and sequence work to take advantage of C++23 language and
library features now that CMake is the build system and the language standard
can be set reliably across all targets.  Both code-clarity and optimization
opportunities are in scope.  Converting away from the `lumalink::span` wrapper
to `std::span` directly is a primary driver.

Status legend:
- `todo`: not started
- `doing`: active
- `done`: completed
- `blocked`: waiting on dependency
- `deferred`: intentionally postponed

---

## Background

The codebase was written against C++17 with a thin compatibility shim
(`src/lumalink/span.h`) that conditionally aliases `std::span` (C++20) or
`tcb::span` (bundled fallback) under the `lumalink::span` name.  Moving to
C++23 as the project baseline removes the need for that compatibility layer and
opens up several newer standard-library and language features.

The full-codebase scan found approximately 58 uses of `lumalink::span` spread
across 10 source headers and the test suite.  Each is a direct candidate for
`std::span` once the standard is bumped.  Additional opportunities exist in
error-handling types, algorithm style, and several small language-feature
improvements.

---

## Tasks

### CPP23-01 — Bump CMake `cxx_std` to 23 `todo`

**File**: `CMakeLists.txt` (root), `test/test_native/CMakeLists.txt`

The root `CMakeLists.txt` currently applies `cxx_std_17` to the
`lumalink::platform` target.  Both the library target and the test executable
must request `cxx_std_23` (or `CMAKE_CXX_STANDARD 23`).

Acceptance criteria:
- `cmake --build build --config Debug` succeeds with no degraded warnings.
- All CTest cases pass after the bump.
- Confirm MSVC sets `/std:c++latest` or `/std:c++23` in the generated project.

This task is a prerequisite for all other tasks in this backlog.

---

### CPP23-02 — Replace `lumalink::span` wrapper with `std::span` `todo`

**Scope**: ~58 usages across:
- `src/lumalink/platform/buffers/ByteStream.h` (~19 sites)
- `src/lumalink/platform/transport/TransportInterfaces.h` (3 sites)
- `src/lumalink/platform/arduino/ArduinoWiFiTransport.h` (6 sites)
- `src/lumalink/platform/posix/PosixSocketTransport.h` (6 sites)
- `src/lumalink/platform/windows/WindowsSocketTransport.h` (6 sites)
- `src/lumalink/platform/posix/PosixFileAdapter.h` (3 sites)
- `src/lumalink/platform/windows/WindowsFileAdapter.h` (3 sites)
- `src/lumalink/platform/arduino/ArduinoFileAdapter.h` (3 sites)
- `src/lumalink/platform/memory/MemoryFileAdapter.h` (3 sites)
- `test/support/include/ByteStreamFixtures.h` (2 sites)
- test `.cpp` files (~3 sites)

**Approach**:
1. Replace every `lumalink::span` and `lumalink::dynamic_extent` with
   `std::span` and `std::dynamic_extent` and update `#include` directives to
   `<span>`.
2. Remove `#include "span.h"` / `#include "tcb_span.hpp"` from all files.
3. Delete `src/lumalink/span.h` and `src/lumalink/tcb_span.hpp` from the repo.
4. Remove the `LUMALINK_USING_STD_SPAN` macro and the fallback shim entirely.

Do not introduce a new `lumalink::span` alias — all call sites should use
`std::span` directly; the wrapper served a compatibility purpose that no longer
exists.

Acceptance criteria:
- No remaining references to `lumalink::span`, `tcb::span`, `tcb_span.hpp`,
  or `span.h` anywhere in `src/` or `test/`.
- All tests pass.

---

### CPP23-03 — Replace `AvailableResult` with `std::expected` `todo`

**File**: `src/lumalink/platform/buffers/Availability.h`,
`src/lumalink/platform/buffers/ByteStream.h`,
`test/test_native/test_transport_native.cpp`

`AvailableResult` is a manual discriminated struct (`AvailabilityState` enum +
`count` + `errorCode`) whose semantics map directly to
`std::expected<std::size_t, int>`:

```cpp
// current
struct AvailableResult {
    AvailabilityState state = AvailabilityState::Exhausted;
    std::size_t count = 0;
    int errorCode = 0;
    constexpr bool hasBytes() const { ... }
    constexpr bool hasError() const { ... }
};
```

Consider:
- `std::expected<std::size_t, AvailabilityError>` where `AvailabilityError`
  encodes both the exhausted/unavailable/error distinction and an optional
  platform error code.
- Factory helpers (`AvailableBytes`, `ExhaustedResult`, etc.) become
  `std::expected` return expressions.

This change touches all `IByteSource::available()` /
`IByteSource::peekAvailable()` implementations and their call sites in
`ByteStream.h` and transport tests.

Acceptance criteria:
- `AvailableResult` and `AvailabilityState` are removed.
- All call sites use `std::expected` monadic operations (`and_then`, `or_else`,
  `value_or`) where appropriate rather than manual `.hasBytes()` checks.
- All tests pass.

---

### CPP23-04 — Replace `UtcTimeResult` with `std::expected` `todo`

**File**: `src/lumalink/platform/time/TimeSource.h`,
`test/test_native/test_time.cpp`

```cpp
// current
struct UtcTimeResult {
    bool ok = false;
    UnixTime time{};
    explicit constexpr operator bool() const { return ok; }
};
```

Replace with `std::expected<UnixTime, std::error_code>` (or a lightweight
platform error enum).  All `ITimeSource::getUtcTime()` implementations and
callers that check `.ok` before accessing `.time` should be updated to use
`std::expected` value access or `value_or`.

Acceptance criteria:
- `UtcTimeResult` is removed.
- `ITimeSource::getUtcTime()` returns `std::expected<UnixTime, E>`.
- All tests pass.

---

### CPP23-05 — Use `std::ranges` for `PathMapper` path normalization `todo`

**File**: `src/lumalink/platform/PathMapper.h`

The `normalizeScopedPath` implementation uses a manual while-loop with
`std::string::find` to split on `/`, resolve `.` and `..` segments, and
rejoin.  The same logic can be expressed using:

- `std::views::split` to tokenize on `/`
- `std::views::filter` to drop empty and `.` segments
- A fold or small accumulating view to handle `..` pop-back
- `std::views::join` (or `std::ranges::fold_left` with a separator) to
  reassemble the cleaned path

The goal is clarity; do not sacrifice the correctness of the existing `.` / `..`
logic.

Acceptance criteria:
- `normalizeScopedPath` is rewritten using ranges/views rather than a
  raw while-loop.
- All filesystem path tests pass.

---

### CPP23-06 — Use `std::ranges` algorithms in test assertions `todo`

**File**: `test/test_native/test_memory_filesystem.cpp`,
`test/test_native/test_filesystem_native.cpp`

Several test functions collect entries into a `std::vector` then scan with
manual loops and flags:

```cpp
bool foundDir = false, foundFile = false;
for (const auto &e : entries) {
    if (e.isDirectory && e.path == "/dir") foundDir = true;
    if (!e.isDirectory && e.path == "/dir/asset.txt") foundFile = true;
}
TEST_ASSERT_TRUE(foundDir);
TEST_ASSERT_TRUE(foundFile);
```

Replace with `std::ranges::any_of` predicates:

```cpp
TEST_ASSERT_TRUE(std::ranges::any_of(entries,
    [](const auto& e){ return e.isDirectory && e.path == "/dir"; }));
```

Acceptance criteria:
- No manual boolean flag loops remain in the listed test files.
- All tests pass.

---

### CPP23-07 — Apply deducing-this to `ByteStream` overload sets `todo`

**File**: `src/lumalink/platform/buffers/ByteStream.h`

`IByteSource` and `IByteTarget` each expose several defaulted overloads
(`readBytes(uint8_t*, size_t)`, `peek(uint8_t*, size_t)`, etc.) that forward
to span-based virtual overloads.  After CPP23-02 lands and all span parameters
become `std::span`, the pointer+size bridge overloads duplicate behaviour.

Evaluate whether the pointer+size overloads can be replaced with a single
deduced-this template that constructs the span and dispatches, e.g.:

```cpp
size_t readBytes(this auto& self, uint8_t* buf, size_t n) {
    return self.read(std::span<uint8_t>(buf, n));
}
```

Only apply where it produces a genuine reduction in boilerplate; do not
restructure virtual dispatch chains beyond what the language permits.

Acceptance criteria:
- At least the `readBytes` and `peek` pointer+size forwarding overloads are
  replaced or removed where the deducing-this form is cleaner.
- No public API surface change.
- All tests pass.

---

### CPP23-08 — Structured bindings sweep `todo`

**Scope**: `src/lumalink/platform/memory/MemoryFileAdapter.h`,
`test/support/include/HttpTestFixtures.h`, any remaining `kv.first` / `kv.second`
pair accesses in the codebase.

Replace remaining `auto& kv : container` loops where `kv.first` / `kv.second`
are accessed with structured bindings (`auto& [key, value] : container`).  This
is a C++17 feature already available but the sweep was not performed.

Acceptance criteria:
- No remaining `.first` / `.second` accesses on map/pair range elements.
- All tests pass.

---

### CPP23-09 — Evaluate `std::flat_map` for `MemoryFileAdapter` node tree `todo`

**File**: `src/lumalink/platform/memory/MemoryFileAdapter.h`

Each `Node` in the in-memory filesystem owns a
`std::map<std::string, std::shared_ptr<Node>> children`.  `std::flat_map`
(C++23) stores keys and values in parallel contiguous vectors, giving better
cache locality for the small, stable child sets typical of a test-fixture
filesystem.

Evaluate whether the replacement is worthwhile given:
- Child sets are small (typically < 20 entries per directory in tests).
- Insertion order-independence is required but not sorted order.
- `shared_ptr` values partially offset the flat-map locality benefit.

If adopted, replace the `children` map type uniformly across all `Node`
instances.

Acceptance criteria:
- Decision recorded (adopt or defer with rationale).
- If adopted: all tests pass with `std::flat_map`.

---

### CPP23-10 — Add `[[assume]]` optimizer hints to hot paths `todo`

**Files**: `src/lumalink/platform/buffers/ByteStream.h`,
`src/lumalink/platform/memory/MemoryFileAdapter.h`

After guarded precondition checks (e.g., `if (position_ >= size_) return`)
the compiler cannot generally prove the invariant holds for subsequent code.
Add `[[assume(condition)]]` in the appropriate post-check positions to give
the optimizer stronger guarantees.

Known candidates:
- `SpanByteSource::read` — after the early-return position/size guard
- `MemoryFileAdapter` node pointer dereference — after null pointer guards

Limit to cases where the missed optimization is measurable or structurally
obvious; do not add `[[assume]]` speculatively throughout the codebase.

Acceptance criteria:
- `[[assume]]` annotations are added only at the identified sites.
- Build produces no new warnings under `/W3` (MSVC) or `-Wall -Wextra` (GCC).
- All tests pass.

---

## Suggested Sequencing

```
CPP23-01  (standard bump — prerequisite for all)
    └─ CPP23-02  (span replacement — highest surface area, unblocks interface cleanup)
           └─ CPP23-07  (deducing-this overloads — cleaner after span is std::span)
    └─ CPP23-03  (AvailableResult → std::expected)
    └─ CPP23-04  (UtcTimeResult → std::expected)
    └─ CPP23-05  (PathMapper ranges)
    └─ CPP23-06  (test assertion ranges)
    └─ CPP23-08  (structured bindings sweep — can be done at any point after 01)
CPP23-09  (flat_map evaluation — independent, can be done any time after 01)
CPP23-10  (assume hints — independent, low priority)
```
