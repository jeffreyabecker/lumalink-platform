# C++23 Modernization Backlog

Purpose: define and sequence the work needed to upgrade the codebase to a C++23
baseline under CMake, remove compatibility layers that are no longer necessary,
and selectively adopt newer standard-library and language features where they
improve code clarity, maintainability, or performance.

Status legend:
- `todo`: not started
- `doing`: active
- `done`: completed
- `blocked`: waiting on dependency
- `deferred`: intentionally postponed

## Implementation Status

Current status: Phase 1 and Phase 2 are complete.

The project now builds as C++23 under CMake, all direct `lumalink::span` usage
has been replaced with `std::span`, and the legacy compatibility files
`src/lumalink/span.h` and `src/lumalink/tcb_span.hpp` have been removed.

The byte-stream availability surface and UTC time-source result surface have
also been modernized onto `std::expected`-based models. Native Windows Debug
builds and the native CTest suite pass after these changes.

The highest-value modernization work is:

- expanding the `std::expected` migration into the remaining result-like APIs
- using standard ranges algorithms where they materially improve clarity

Lower-priority work exists for structured-bindings cleanup, flat associative
containers, and targeted optimizer hints.

## Design Intent

- Use C++23 to simplify existing code, not to introduce novelty for its own sake.
- Remove compatibility abstractions whose only purpose was supporting pre-C++20 compilers.
- Prefer direct use of standard-library facilities over project-local wrappers when the standard type is now the actual contract.
- Prioritize changes that improve readability and reduce bespoke error-handling code before micro-optimizations.
- Keep public behavior stable while modernizing implementation details.

## Scope

- Root and test CMake language-standard settings
- All current `lumalink::span` usage sites in `src/` and `test/`
- Error-handling result types in buffer and time abstractions
- `PathMapper` normalization logic and selected test assertion loops
- Targeted cleanup in `ByteStream`, `MemoryFileAdapter`, and supporting test fixtures

## Non-Goals

- Do not perform broad stylistic churn unrelated to C++23 adoption
- Do not rewrite working code to use every new feature when the gain is marginal
- Do not introduce new compatibility aliases that preserve `lumalink::span`
- Do not convert the library surface to C++ modules as part of this backlog
- Do not pursue `std::mdspan` or `std::print` adoption where the codebase has no meaningful fit today

## Architectural Rules

- `std::span` replaces the wrapper directly; do not retain a project-owned alias layer.
- Result-type modernizations must preserve existing semantics before attempting API cleanup beyond the call sites in scope.
- Apply ranges only where the resulting code is clearer than the existing loop-based form.
- Deducing-this should only be used where it reduces overload boilerplate without obscuring dispatch behavior.
- Performance-oriented features such as `[[assume]]` or `std::flat_map` require a concrete rationale, not speculative adoption.

## Work Phases

## Phase 1 - Baseline Upgrade And Span Surface

| ID | Status | Task | Depends On | Definition of Done |
|---|---|---|---|---|
| CPP23-01 | done | Bump the project CMake configuration from C++17 to C++23 for both the library and native test targets | none | The root and test CMake files request C++23, native builds succeed, and CTest passes with the new baseline |
| CPP23-02 | done | Replace all `lumalink::span` and `lumalink::dynamic_extent` usage with `std::span` and `std::dynamic_extent` throughout `src/` and `test/` | CPP23-01 | No production or test code references `lumalink::span`, `tcb::span`, `span.h`, or `tcb_span.hpp`, and all tests pass |
| CPP23-03 | done | Remove the obsolete compatibility files `src/lumalink/span.h` and `src/lumalink/tcb_span.hpp` after all call sites are migrated | CPP23-02 | The compatibility shim files are deleted and nothing in the build or source tree depends on them |

## Phase 2 - Result Type Modernization

| ID | Status | Task | Depends On | Definition of Done |
|---|---|---|---|---|
| CPP23-04 | done | Replace `AvailableResult` and `AvailabilityState` in the byte-stream layer with a `std::expected`-based result model | CPP23-01 | `Availability.h` no longer defines the bespoke result struct and all byte-source call sites compile and pass tests using the new expected-based surface |
| CPP23-05 | done | Replace `UtcTimeResult` in the time abstraction with `std::expected<UnixTime, E>` | CPP23-01 | `TimeSource.h` uses `std::expected` for UTC retrieval and all callers are updated without behavior regressions |

## Phase 3 - Clarity Improvements With Standard Algorithms

| ID | Status | Task | Depends On | Definition of Done |
|---|---|---|---|---|
| CPP23-06 | todo | Rewrite `PathMapper` normalization around ranges/views if the final implementation is clearer than the current manual split-and-join loop | CPP23-01 | `PathMapper.h` uses ranges-based logic without reducing correctness, and all path-related tests pass |
| CPP23-07 | todo | Replace manual boolean-flag scanning loops in the native filesystem tests with `std::ranges::any_of` or equivalent standard algorithms | CPP23-01 | The targeted test files no longer use manual flag loops where a direct standard algorithm is clearer |
| CPP23-08 | todo | Sweep remaining pair-based loops in production and test code to use structured bindings where they currently access `.first` and `.second` | CPP23-01 | Remaining map/pair iteration sites use structured bindings where it improves readability and no tests regress |

## Phase 4 - Interface Cleanup And Focused Optimization

| ID | Status | Task | Depends On | Definition of Done |
|---|---|---|---|---|
| CPP23-09 | todo | Evaluate deducing-this in `ByteStream.h` for pointer-plus-size forwarding overloads after the span migration lands | CPP23-02 | The relevant overload set is simplified only where the deducing-this form is clearer and the public API behavior remains unchanged |
| CPP23-10 | todo | Evaluate `std::flat_map` for `MemoryFileAdapter` node children and either adopt it or explicitly defer it with rationale | CPP23-01 | A recorded decision exists, and if adopted the node tree compiles and all tests pass |
| CPP23-11 | todo | Add targeted `[[assume]]` annotations in validated hot paths such as guarded `ByteStream` and memory-filesystem code | CPP23-01 | `[[assume]]` is used only at justified sites, introduces no new warnings, and all tests pass |

## Phase 5 - Deferred Research

| ID | Status | Task | Depends On | Definition of Done |
|---|---|---|---|---|
| CPP23-12 | deferred | Evaluate C++ modules for the native-host build path after embedded target work reduces preprocessor-heavy platform branching | CPP23-01 | A short design decision is recorded covering build-system viability, downstream-consumer impact, and whether modules should remain deferred |
