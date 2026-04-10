# Embedded Platform Target Implementations Backlog

Purpose: define and sequence the work needed to replace the Arduino SDK layer in
`src/lumalink/platform/arduino/` with native SDK implementations for three
embedded targets:

- **Raspberry Pi Pico / Pico-W** — [pico-sdk](https://github.com/raspberrypi/pico-sdk)
- **ESP8266** — [ESP8266 RTOS SDK](https://docs.espressif.com/projects/esp8266-rtos-sdk/en/latest/get-started/index.html)
- **ESP32** — [ESP-IDF](https://github.com/espressif/esp-idf)

Prerequisite: the CMake build system migration (backlog 001) must be complete.
All three targets are delivered as independent, parallel workstreams that share
common design decisions described below.

Status legend:
- `todo`: not started
- `doing`: active
- `done`: completed
- `blocked`: waiting on dependency
- `deferred`: intentionally postponed

---

## Background

The current Arduino layer lives entirely in three headers:

| File | Arduino API consumed |
|---|---|
| `platform/arduino/ArduinoTime.h` | `millis()`, `time()`, `NTP` (arduino-pico / ESP8266 / WiFiNTP) |
| `platform/arduino/ArduinoWiFiTransport.h` | `WiFiClient`, `WiFiServer`, `WiFiUDP`, `IPAddress` |
| `platform/arduino/ArduinoFileAdapter.h` | `FS`, `File` (`LittleFS`, `SPIFFS`, etc.) |

Factory dispatch is in two headers that select the implementation at
preprocessor time:

| File | `#if` guard resolved |
|---|---|
| `platform/ClockFactory.h` | `ARDUINO` → `ArduinoClock` |
| `platform/TransportFactory.h` | `ARDUINO` → `ArduinoWiFiTransportFactory` |

Each target workstream below replaces the Arduino branch with a native-SDK
branch in each of these five files, adds a new `platform/<target>/` directory,
and wires a CMake toolchain / build integration.

---

## Shared Design Decisions

These rules apply to all three target workstreams.

### Platform detection macros

Use the SDK-provided detection symbols rather than inventing project-specific
ones. Precedence in factory `#if` chains must be established before the generic
`ARDUINO` fallback is removed:

| Target | Reliable detection guard |
|---|---|
| Pico / Pico-W | `PICO_SDK_VERSION_MAJOR` (defined by pico-sdk headers) |
| ESP32 (ESP-IDF) | `ESP_PLATFORM` and `CONFIG_IDF_TARGET_ESP32` (or family variants) |
| ESP8266 (RTOS SDK) | `ESP_PLATFORM` and `CONFIG_IDF_TARGET_ESP8266` |

The factory headers become a cascade:

```
#if defined(PICO_SDK_VERSION_MAJOR)
  // pico
#elif defined(ESP_PLATFORM) && defined(CONFIG_IDF_TARGET_ESP8266)
  // esp8266
#elif defined(ESP_PLATFORM)
  // esp32 family
#elif defined(ARDUINO)
  // legacy arduino fallback — removed once all targets are implemented
#elif defined(_WIN32)
  // windows native
#else
  // posix native
#endif
```

### WiFi / network initialisation lifecycle

The transport factory does not own WiFi or network stack initialisation; that is
the responsibility of the application layer before constructing a
`TransportFactory`. The platform-specific implementation headers must document
this contract explicitly with a comment.

### Filesystem VFS shortcut (ESP targets)

Both ESP8266 RTOS SDK and ESP-IDF expose a POSIX-compatible VFS layer. After a
VFS partition is registered (`esp_vfs_spiffs_register`,
`esp_vfs_littlefs_register`, or `esp_vfs_fat_spiflash_mount`), the existing
`platform/posix/PosixFileAdapter.h` can be reused directly without a new
adapter class. The ESP filesystem tasks below are therefore lighter: mount
configuration and documentation, not new adapter code.

### Integer time arithmetic

All monotonic clock implementations must return `MonotonicMillis` as an integer
(`uint64_t` milliseconds). Do not accumulate floating-point intermediate values
in the conversion from the SDK's native tick or microsecond counter.

---

## Workstream A — Raspberry Pi Pico / Pico-W (pico-sdk)

### A1 — CMake toolchain and build integration  `todo`

- Add `cmake/toolchains/pico.cmake` that sets `PICO_SDK_PATH` from an
  environment variable or CMake cache entry and includes
  `pico_sdk_import.cmake`.
- Add a top-level CMake option `LUMALINK_TARGET_PICO` (OFF by default) that
  activates the Pico toolchain and links the required pico-sdk components:
  `pico_stdlib`, `pico_time`, `hardware_rtc`, and (when Pico-W) `pico_cyw43_arch_lwip`.
- The library target must not pull in pico-sdk headers unless
  `LUMALINK_TARGET_PICO` is ON.

### A2 — Monotonic clock (`PicoMonotonicClock`)  `todo`

- Add `platform/pico/PicoTime.h`.
- `PicoMonotonicClock::monotonicNow()` returns
  `MonotonicMillis{to_ms_since_boot()}` using `<pico/time.h>`.
- `to_ms_since_boot()` returns `uint32_t` and wraps at ~49 days; the adapter
  must cast to `uint64_t` before returning. A wrap-detection extension is
  deferred; document the limitation.

### A3 — UTC clock (`PicoUtcClock`)  `todo`

- Add `PicoUtcClock` to `platform/pico/PicoTime.h`.
- The Pico base board has no RTC oscillator; `utcNow()` returns `std::nullopt`
  until the application sets the time via the settable-clock interface or an
  NTP sync writes through the hardware RTC.
- On Pico-W: when `pico_cyw43_arch_lwip` is linked, expose a `beginSntp()`
  helper that calls `sntp_setservername` and `sntp_init()` (lwIP's SNTP
  client). The SNTP callback writes the received time and makes `utcNow()`
  available.
- The Pico hardware RTC (`hardware_rtc`) provides the backing store;
  `rtc_get_datetime()` converts to `UnixTime`. Implement the conversion using
  standard `<ctime>` (`std::mktime` on a `tm` struct).
- `PicoClock` aggregates `PicoMonotonicClock` and `PicoUtcClock`.

### A4 — Transport (`PicoLwipTransportFactory`) — Pico-W only  `todo`

- Add `platform/pico/PicoLwipTransport.h`.
- Pico-W exposes BSD socket headers via `<lwip/sockets.h>` and
  `<lwip/netdb.h>`. The implementation is structurally identical to
  `PosixSocketTransport` but must include the lwIP headers rather than the POSIX
  ones.
- Factor out the common BSD socket logic into a shared internal template or
  non-owning base class in `platform/posix/BsdSocketTransport.h` that can be
  included by both the POSIX and Pico implementations. The refactor is scoped
  to this task only; do not touch Windows transport.
- `PicoLwipTransportFactory` satisfies `IsStaticTransportFactoryV`.
- Document that `cyw43_arch_init()` and network bring-up must complete before
  the factory is used.
- The bare Pico (no WiFi) has no socket transport; `ClockFactory.h` and
  `TransportFactory.h` must guard the transport selection on
  `PICO_CYW43_SUPPORTED` in addition to `PICO_SDK_VERSION_MAJOR`.

### A5 — Filesystem  `todo`

- Pico-sdk does not ship LittleFS; the de-facto library is
  [littlefs-lib](https://github.com/lurk101/pico-littlefs) or the upstream
  [littlefs](https://github.com/littlefs-project/littlefs) adapted for RP2040
  flash.
- Add `platform/pico/PicoLittleFsAdapter.h` wrapping `lfs_file_open`,
  `lfs_file_read`, `lfs_file_write`, `lfs_file_close`, `lfs_stat`,
  `lfs_dir_open`, `lfs_dir_read` to implement `IFile` and `IFileSystem`.
- LittleFS has no POSIX VFS bridge on Pico; the adapter must be written
  directly against the littlefs C API.
- Path normalisation reuses `PosixPathMapper` (already platform-independent).

### A6 — Factory dispatch update  `todo`

- Update `ClockFactory.h` to add the `PICO_SDK_VERSION_MAJOR` branch selecting
  `pico::PicoClock`, `pico::PicoMonotonicClock`, `pico::PicoUtcClock`.
- Update `TransportFactory.h` to add the `PICO_SDK_VERSION_MAJOR` /
  `PICO_CYW43_SUPPORTED` branch selecting `pico::PicoLwipTransportFactory`.
- Add a `FileSystemFactory.h` (new file) that dispatches filesystem adapters
  with the same `#if` cascade if one does not already exist.

---

## Workstream B — ESP8266 (ESP8266 RTOS SDK)

### B1 — CMake toolchain and build integration  `todo`

- Add `cmake/toolchains/esp8266.cmake` that sets the Xtensa GCC cross-compiler
  from `$ENV{IDF_PATH}` (ESP8266 RTOS SDK uses a distinct toolchain from
  ESP-IDF).
- The ESP8266 RTOS SDK uses a `make menuconfig` / `idf.py` (legacy) build
  model rather than modern ESP-IDF CMake components. Provide a
  `cmake/esp8266_component.cmake` helper that wraps the SDK's
  `idf_component_register` shim so this library can be registered as an IDF
  component.
- Add a top-level option `LUMALINK_TARGET_ESP8266` (OFF by default).

### B2 — Monotonic clock (`Esp8266MonotonicClock`)  `todo`

- Add `platform/esp8266/Esp8266Time.h`.
- Use `esp_timer_get_time()` (returns `int64_t` microseconds since boot) from
  `<esp_timer.h>` and divide by 1000 using integer arithmetic to produce
  `MonotonicMillis`.
- Alternatively, `xTaskGetTickCount() * portTICK_PERIOD_MS` from FreeRTOS gives
  a millisecond tick directly at the cost of tick resolution; prefer
  `esp_timer_get_time` for higher accuracy.

### B3 — UTC clock (`Esp8266UtcClock`)  `todo`

- Add `Esp8266UtcClock` to `platform/esp8266/Esp8266Time.h`.
- The ESP8266 RTOS SDK ships an SNTP client (`<apps/sntp/sntp.h>`).
  After `sntp_setservername()` and `sntp_init()`, `time(nullptr)` returns the
  synchronized UTC time.
- `utcNow()` validates the result against `kMinimumSaneUnixTime` (reuse
  or reference the constant from `arduino::detail`) before returning.
- Expose a `beginSntp(const char* server)` helper; document that the caller
  must have brought up the WiFi stack before calling it.
- `Esp8266Clock` aggregates `Esp8266MonotonicClock` and `Esp8266UtcClock`.

### B4 — Transport (`Esp8266SocketTransportFactory`)  `todo`

- The ESP8266 RTOS SDK exposes BSD sockets (`<sys/socket.h>`, `<netdb.h>`)
  through lwIP's socket compat layer. The implementation can share the BSD
  socket base introduced in A4 if workstream A has already landed; otherwise
  implement independently using the same pattern as `PosixSocketTransport`.
- Add `platform/esp8266/Esp8266SocketTransport.h`.
- No VFS-level initialisation is required for sockets; document that the
  application must have completed WiFi and TCP/IP initialisation via
  `tcpip_adapter_init()` / `esp_netif_init()`.

### B5 — Filesystem  `todo`

- Register a SPIFFS partition with `esp_vfs_spiffs_register()` to mount it
  under a POSIX path (e.g., `/spiffs`).
- After mount, the existing `platform/posix/PosixFileAdapter.h` satisfies the
  `IFileSystem` contract without modification.
- Add a thin `platform/esp8266/Esp8266Filesystem.h` that provides an
  `Esp8266SpiffsFilesystem` factory function:
  `std::unique_ptr<IFileSystem> MountSpiffs(const esp_vfs_spiffs_conf_t& conf)`
  which calls `esp_vfs_spiffs_register`, checks the result, constructs and
  returns a `PosixFileSystem` rooted at `conf.base_path`.
- Document that `esp_vfs_littlefs_register` is a drop-in alternative; the
  factory function design is the same.

### B6 — Factory dispatch update  `todo`

- Update `ClockFactory.h` and `TransportFactory.h` with the
  `ESP_PLATFORM && CONFIG_IDF_TARGET_ESP8266` branch.
- The filesystem factory (see A6) gains an ESP8266 branch; it cannot use the
  VFS adapter without a registered mount, so the factory dispatches to
  `Esp8266SpiffsFilesystem` (caller provides mount config at startup).

---

## Workstream C — ESP32 (ESP-IDF)

### C1 — CMake toolchain and build integration  `todo`

- Add `cmake/toolchains/esp32.cmake` that configures the Xtensa (ESP32) or
  RISC-V (ESP32-C3) cross-compiler from `$ENV{IDF_PATH}`.
- ESP-IDF uses its own CMake build system (`idf.py build`). Provide a
  `cmake/esp32_component.cmake` helper similar to the ESP8266 one registering
  the library as a component with `idf_component_register`.
- Add a top-level option `LUMALINK_TARGET_ESP32` (OFF by default).
- The ESP32 family covers multiple silicon variants (ESP32, S2, S3, C3, H2);
  all use the same ESP-IDF API surface targeted here. No per-variant
  specialisation is required at this stage.

### C2 — Monotonic clock (`Esp32MonotonicClock`)  `todo`

- Add `platform/esp32/Esp32Time.h`.
- Use `esp_timer_get_time()` from `<esp_timer.h>` (same API as ESP8266 RTOS
  SDK); return `MonotonicMillis{static_cast<uint64_t>(esp_timer_get_time()) / 1000}`.
- Integer division; no floating point.

### C3 — UTC clock (`Esp32UtcClock`)  `todo`

- Add `Esp32UtcClock` to `platform/esp32/Esp32Time.h`.
- Use `esp_sntp_init()` / `esp_sntp_setservername()` from `<esp_sntp.h>` (the
  modern ESP-IDF SNTP API since IDF v5; fall back to `<lwip/apps/sntp.h>` for
  IDF v4).
- After SNTP sync, `time(nullptr)` is valid; validate against
  `kMinimumSaneUnixTime`.
- Expose `beginSntp(const char* server)` with the same contract as ESP8266.
- `Esp32Clock` aggregates both.

### C4 — Transport (`Esp32SocketTransportFactory`)  `todo`

- ESP-IDF exposes full BSD sockets via `<sys/socket.h>` and `<netdb.h>` through
  `esp_netif` + lwIP. The implementation shares the BSD socket base (A4) or is
  copied from the POSIX implementation with adjusted includes.
- Add `platform/esp32/Esp32SocketTransport.h`.
- Document that `esp_netif_init()`, `esp_event_loop_create_default()`, and WiFi
  / Ethernet bring-up must complete before the factory is used.

### C5 — Filesystem  `todo`

- Same VFS shortcut as ESP8266 (B5).
- Add `platform/esp32/Esp32Filesystem.h` providing `MountSpiffs` and
  `MountFat` factory functions using `esp_vfs_spiffs_register` and
  `esp_vfs_fat_spiflash_mount` respectively; both return a
  `PosixFileSystem` rooted at the registered base path.
- Document that `esp_vfs_littlefs_register` (via `idf_component_manager`
  component `joltwallet/littlefs`) is a supported alternative.

### C6 — Factory dispatch update  `todo`

- Update `ClockFactory.h` and `TransportFactory.h` with the `ESP_PLATFORM`
  (without `CONFIG_IDF_TARGET_ESP8266`) branch selecting ESP32 types.
- The filesystem factory for ESP32 mirrors the ESP8266 entry point; both
  dispatch to their respective `Mount*` helpers.

---

## Cross-cutting Tasks

### X1 — Remove the `ARDUINO` branch from factory headers  `todo`

After all three native-SDK workstreams are complete and verified, remove the
`ARDUINO` branch from `ClockFactory.h` and `TransportFactory.h`. The
`platform/arduino/` directory is then dead code and should be deleted.

Defer this task until the last workstream passes an integration build to avoid
breaking the Arduino path prematurely.

### X2 — BSD socket shared base (if not already done in A4)  `todo`

If workstream A is delivered after B or C, factor out the common BSD socket
logic shared by POSIX, ESP8266, ESP32, and Pico-W implementations to eliminate
duplication. The shared base lives in `platform/posix/BsdSocketTransportBase.h`
(a class template or CRTP base, not a full implementation class).

### X3 — CMake preset integration  `todo`

Add a `CMakePresets.json` (or extend an existing one) with configure presets
for each embedded target: `pico`, `esp8266`, `esp32`. Each preset sets the
corresponding `LUMALINK_TARGET_*` option and points to the matching toolchain
file. This gives contributors a single entry point for each embedded build.

---

## Non-Goals

- Do not implement BLE, Bluetooth, or peripheral driver layers.
- Do not implement WiFi credential management or provisioning.
- Do not implement an OTA update layer.
- Do not add Arduino-SDK compatibility shims or wrappers; the Arduino layer is
  replaced, not extended.
- Do not support bare Pico (no WiFi) transport; only Pico-W gets a socket
  factory.
