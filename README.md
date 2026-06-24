# obcsw — Satellite On-Board Computer Software

Production-grade C++17 OBC middleware demonstrating the core subsystems
found in real satellite flight software: fault detection, JSON/YAML
serialization, thread-safe command queuing, and UDP telemetry downlink.

Built to target embedded-Linux satellite platforms.  

Deployed at [https://zte0.github.io/On-Board-Computer-Software-Stack/](https://zte0.github.io/On-Board-Computer-Software-Stack/)

---

## Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                         OBC Main Loop                        │
│  generateFrame() → FaultDetector → TelemetryDispatcher ─ UDP │
│                         ↓                                    │
│              CommandQueue ← uplink JSON                      │
│                    ↓                                         │
│              CommandHandler → registered callbacks           │
└──────────────────────────────────────────────────────────────┘
```


| Component             | File                                        | Responsibility                                                     |
| --------------------- | ------------------------------------------- | ------------------------------------------------------------------ |
| `FaultDetector`       | `src/health/FaultDetector.cpp`              | Per-frame threshold evaluation; fires typed `FaultEvent` callbacks |
| `TelemetryDispatcher` | `src/telemetry/TelemetryDispatcher.cpp`     | Background thread, UDP downlink, drop counter                      |
| `TelemetrySerializer` | `src/serialization/TelemetrySerializer.cpp` | JSON ↔ `TelemetryFrame` (nlohmann/json)                            |
| `CommandDeserializer` | `src/serialization/CommandDeserializer.cpp` | JSON → `Command` with opcode allowlist                             |
| `CommandHandler`      | `src/command/CommandHandler.cpp`            | Opcode-keyed dispatch table                                        |
| `CommandQueue`        | `src/command/CommandQueue.cpp`              | Thread-safe bounded FIFO with `condition_variable`                 |
| `ConfigLoader`        | `src/serialization/ConfigLoader.cpp`        | YAML → `SatelliteConfig` (yaml-cpp)                                |


---

## Build

### Prerequisites

- CMake ≥ 3.16
- GCC ≥ 11 or Clang ≥ 14 (C++17)
- Internet access for FetchContent (nlohmann/json, yaml-cpp, Catch2)

### Quick start

```bash
git clone https://github.com/yourhandle/obcsw.git
cd obcsw
./scripts/build.sh          # Release build
./scripts/build.sh debug    # Debug + ASan/UBSan
./scripts/build.sh test     # Build + run all tests
```

Binary lands at `build/release/obcsw`.

### Manual CMake

```bash
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --parallel
cd build/release && ctest --output-on-failure
```

---

## Running

```bash
./build/release/obcsw config/satellite.yaml
```

The OBC loop will:

1. Load config from the YAML file (falls back to defaults if not found)
2. Start the UDP telemetry dispatcher
3. Inject three demo commands (NOOP, SET_MODE, PAYLOAD_ON)
4. Loop, printing telemetry frames and triggering fault callbacks

Press **Ctrl+C** to shut down gracefully.

---

## Tests

27 unit tests across all subsystems using Catch2 v2:

```
FaultDetector       9 tests — threshold coverage for all fault codes
TelemetrySerializer 4 tests — round-trip JSON, batch, error paths
CommandDeserializer 6 tests — valid/invalid opcodes, missing fields
CommandHandler      3 tests — dispatch, REJECTED paths
CommandQueue        5 tests — push/pop, bounds, blocking, threading
─────────────────────────────────────────────────────────
Total               27 passed, 0 failed (0.22 s)
```

---

## Configuration (`config/satellite.yaml`)

```yaml
satellite:
  name: "LOFT-1"
  mission_id: "MISSION-ALPHA-2025"

health:
  fault_thresholds:
    battery_voltage_min_v: 6.5
    battery_voltage_max_v: 8.4
    temperature_max_c: 85.0
    cpu_usage_max_pct: 90
```

All fields have safe defaults — a minimal YAML only needs `satellite.name`.

---

## Tech Stack

- **C++17** — `std::optional`, structured bindings, `if constexpr`
- **CMake 3.16+** — FetchContent for zero-install dependency management
- **nlohmann/json 3.11** — header-only JSON serialization
- **yaml-cpp 0.8** — YAML config parsing
- **Catch2 v2** — BDD-style unit tests registered with CTest
- **POSIX sockets** — UDP downlink (Linux / Embedded Linux)
- **pthreads** — `std::thread`, `std::mutex`, `std::condition_variable`

