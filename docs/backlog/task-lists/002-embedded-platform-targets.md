# Embedded Platform Target Implementations Backlog

Purpose: define and sequence the work needed to replace the Arduino SDK layer in
the platform abstraction with native SDK implementations for Raspberry Pi Pico /
Pico-W, ESP8266 RTOS SDK, and ESP-IDF on ESP32-family targets, while keeping
native host builds on the CMake path and deferring non-essential embedded
ecosystem work.

Status legend:
- `todo`: not started
- `doing`: active
- `done`: completed
- `blocked`: waiting on dependency
- `deferred`: intentionally postponed

## Implementation Status

Current status: not started.

The current embedded-facing layer is still the Arduino implementation under
`src/lumalink/platform/arduino/`. Platform selection is handled by factory
headers that dispatch on `ARDUINO`, `_WIN32`, and the default POSIX path. No
native SDK implementation exists yet for Pico, ESP8266 RTOS SDK, or ESP-IDF.

The current Arduino API surface is concentrated in:

- `src/lumalink/platform/arduino/ArduinoTime.h`
- `src/lumalink/platform/arduino/ArduinoWiFiTransport.h`
- `src/lumalink/platform/arduino/ArduinoFileAdapter.h`
- `src/lumalink/platform/ClockFactory.h`
- `src/lumalink/platform/TransportFactory.h`

The ESP filesystem path can likely reuse the existing POSIX filesystem adapter
through each SDK's VFS layer. Pico does not have that shortcut and will require
a direct filesystem adapter.

## Design Intent

- Replace Arduino-SDK dependencies with native target SDKs rather than adding compatibility shims.
- Keep target detection based on SDK-provided preprocessor symbols instead of project-owned macros.
- Preserve the existing platform abstraction boundaries: time, transport, filesystem, and factory dispatch.
- Reuse existing platform code where the target SDK already exposes POSIX-compatible facilities.
- Keep monotonic time integer-based throughout; do not introduce floating-point time conversion.
- Treat WiFi and network stack startup as an application concern, not a transport-factory concern.

## Scope

- New embedded platform implementations under `src/lumalink/platform/pico/`, `src/lumalink/platform/esp8266/`, and `src/lumalink/platform/esp32/`
- Embedded-target CMake toolchain integration and preset support
- Factory dispatch changes in `ClockFactory.h`, `TransportFactory.h`, and a filesystem factory surface if needed
- Embedded transport implementations for Pico-W, ESP8266, and ESP32
- Embedded time implementations for all three targets
- Filesystem integration for Pico via LittleFS and for ESP targets via VFS-backed POSIX reuse

## Non-Goals

- Do not implement BLE, Bluetooth, or peripheral driver layers
- Do not implement WiFi provisioning, credential management, or connection orchestration
- Do not implement OTA update infrastructure
- Do not preserve the Arduino layer as a long-term supported compatibility target
- Do not add a transport path for bare Pico without WiFi support

## Architectural Rules

- Use SDK-defined feature-detection macros: `PICO_SDK_VERSION_MAJOR`, `ESP_PLATFORM`, and `CONFIG_IDF_TARGET_*`.
- Establish deterministic factory `#if` precedence before removing the `ARDUINO` branch.
- Reuse `platform/posix/PosixFileAdapter.h` for ESP targets after VFS mount registration instead of creating parallel filesystem adapters.
- Factor shared BSD-socket logic once it is needed across more than one embedded target; do not duplicate transport code indefinitely.
- Keep embedded platform headers from polluting unrelated native builds; target SDK headers must only be visible when that target is enabled.
- FileSystem factory dispatch must follow the same platform-selection strategy as the clock and transport factories.

## Work Phases

## Phase 1 - Shared Platform Architecture

| ID | Status | Task | Depends On | Definition of Done |
|---|---|---|---|---|
| EMBED-01 | todo | Define the embedded target detection and factory-dispatch precedence using `PICO_SDK_VERSION_MAJOR`, `ESP_PLATFORM && CONFIG_IDF_TARGET_ESP8266`, `ESP_PLATFORM`, `ARDUINO`, `_WIN32`, and POSIX fallback | none | The intended factory cascade is documented and accepted as the authoritative platform-selection order for all embedded work |
| EMBED-02 | todo | Add or define a filesystem factory dispatch surface aligned with the existing clock and transport factory pattern | EMBED-01 | A documented and implemented factory entry point exists for selecting filesystem adapters by platform without ad-hoc call-site branching |
| EMBED-03 | todo | Document and enforce the rule that WiFi and network stack initialization is application-owned rather than transport-factory-owned | EMBED-01 | Each embedded transport implementation documents its network initialization prerequisites and no factory code attempts to provision WiFi |

## Phase 2 - Raspberry Pi Pico / Pico-W

| ID | Status | Task | Depends On | Definition of Done |
|---|---|---|---|---|
| EMBED-04 | todo | Add `cmake/toolchains/pico.cmake` and a top-level `LUMALINK_TARGET_PICO` option that activates Pico SDK integration without affecting unrelated builds | EMBED-01 | Pico-specific CMake configuration is isolated behind the option, imports `pico_sdk_import.cmake`, and only adds Pico headers and libraries when enabled |
| EMBED-05 | todo | Implement `platform/pico/PicoTime.h` with `PicoMonotonicClock`, `PicoUtcClock`, and aggregate `PicoClock` | EMBED-04 | `monotonicNow()` returns integer milliseconds from `<pico/time.h>`, `utcNow()` uses RTC-backed state, and Pico-W SNTP support is exposed through an explicit helper |
| EMBED-06 | todo | Implement `platform/pico/PicoLwipTransport.h` for Pico-W using lwIP BSD sockets and gate transport availability on `PICO_CYW43_SUPPORTED` | EMBED-04, EMBED-03 | Pico-W transport satisfies the transport-factory contract, requires prior network bring-up, and the bare Pico path does not falsely expose socket transport |
| EMBED-07 | todo | Implement `platform/pico/PicoLittleFsAdapter.h` against the LittleFS C API for Pico storage | EMBED-04, EMBED-02 | Pico filesystem support provides `IFile` and `IFileSystem` implementations without relying on a POSIX VFS bridge |
| EMBED-08 | todo | Wire Pico clock, transport, and filesystem implementations into the platform factories | EMBED-05, EMBED-06, EMBED-07 | Factory dispatch selects Pico types from SDK macros, including the WiFi-only transport guard for Pico-W |

## Phase 3 - ESP8266 RTOS SDK

| ID | Status | Task | Depends On | Definition of Done |
|---|---|---|---|---|
| EMBED-09 | todo | Add `cmake/toolchains/esp8266.cmake`, target option `LUMALINK_TARGET_ESP8266`, and component-registration glue for the ESP8266 RTOS SDK build model | EMBED-01 | The library can be registered into the ESP8266 SDK build flow without affecting native-host or other embedded builds |
| EMBED-10 | todo | Implement `platform/esp8266/Esp8266Time.h` with monotonic and UTC clocks backed by `esp_timer_get_time()` and SNTP/time APIs | EMBED-09 | Monotonic time is integer-based, UTC is validated before use, and SNTP setup is explicit rather than automatic |
| EMBED-11 | todo | Implement `platform/esp8266/Esp8266SocketTransport.h` using the SDK's BSD socket compatibility layer | EMBED-09, EMBED-03 | ESP8266 transport satisfies the transport factory and documents that network stack initialization must be complete before use |
| EMBED-12 | todo | Add `platform/esp8266/Esp8266Filesystem.h` that mounts SPIFFS or LittleFS through VFS and returns a POSIX-backed filesystem instance | EMBED-09, EMBED-02 | ESP8266 filesystem support reuses `PosixFileAdapter.h` after successful VFS registration and does not duplicate filesystem adapter logic |
| EMBED-13 | todo | Wire ESP8266 clock, transport, and filesystem support into the platform factories | EMBED-10, EMBED-11, EMBED-12 | Factory dispatch recognizes `ESP_PLATFORM && CONFIG_IDF_TARGET_ESP8266` and routes to ESP8266 types consistently |

## Phase 4 - ESP32 / ESP-IDF

| ID | Status | Task | Depends On | Definition of Done |
|---|---|---|---|---|
| EMBED-14 | todo | Add `cmake/toolchains/esp32.cmake`, target option `LUMALINK_TARGET_ESP32`, and ESP-IDF component-registration integration | EMBED-01 | The library can be consumed as an ESP-IDF component for ESP32-family targets without altering non-ESP builds |
| EMBED-15 | todo | Implement `platform/esp32/Esp32Time.h` with monotonic and UTC clocks backed by `esp_timer_get_time()` and ESP-IDF SNTP APIs | EMBED-14 | Monotonic time remains integer-based, UTC is only surfaced after sane-time validation, and the implementation handles the supported ESP-IDF API surface |
| EMBED-16 | todo | Implement `platform/esp32/Esp32SocketTransport.h` using ESP-IDF BSD sockets | EMBED-14, EMBED-03 | ESP32 transport integrates with the transport factory and documents required `esp_netif` and event-loop initialization prerequisites |
| EMBED-17 | todo | Add `platform/esp32/Esp32Filesystem.h` providing VFS-backed filesystem mount helpers for SPIFFS and FAT | EMBED-14, EMBED-02 | ESP32 filesystem support returns POSIX-backed filesystem objects after successful VFS registration and supports the intended mount variants |
| EMBED-18 | todo | Wire ESP32 clock, transport, and filesystem support into the platform factories | EMBED-15, EMBED-16, EMBED-17 | Factory dispatch recognizes the non-ESP8266 `ESP_PLATFORM` path and selects ESP32-family types consistently |

## Phase 5 - Cross-Cutting Refactors And Cleanup

| ID | Status | Task | Depends On | Definition of Done |
|---|---|---|---|---|
| EMBED-19 | todo | Factor shared BSD socket transport behavior into `platform/posix/BsdSocketTransportBase.h` or equivalent once at least two embedded targets need it | EMBED-06, EMBED-11, EMBED-16 | Shared socket logic is centralized without widening the Windows transport scope or introducing an unnecessary inheritance hierarchy |
| EMBED-20 | todo | Add `CMakePresets.json` entries for `pico`, `esp8266`, and `esp32` configure flows | EMBED-04, EMBED-09, EMBED-14 | Each embedded target has a named configure preset that sets the corresponding target option and toolchain file |
| EMBED-21 | todo | Remove the `ARDUINO` branch from the platform factories and delete `src/lumalink/platform/arduino/` after all replacement targets are implemented and verified | EMBED-08, EMBED-13, EMBED-18 | Factory headers no longer reference Arduino, the Arduino platform directory is removed, and no embedded target regresses as a result |
