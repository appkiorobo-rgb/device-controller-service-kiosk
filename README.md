# Device Controller Service

C++ Windows Service for hardware control platform in kiosk systems.

## Architecture

This service follows a strict layered architecture:

1. **Service Core** - Orchestrates devices, manages state machines, applies recovery logic
2. **Device Abstraction Layer** - Interfaces: `ICamera`, `IPrinter`, `IPaymentTerminal`
3. **Vendor Adapter Layer** - Wraps vendor SDKs (to be implemented)
4. **IPC Layer** - Named Pipes transport, JSON messages, command/response/event model

## Building

### Prerequisites
- CMake 3.20 or higher
- C++20 compatible compiler (MSVC 2019+, GCC 10+, Clang 12+)
- Windows SDK (for Windows Service API)

### Build Steps

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

## Installation

### Install Service
```bash
DeviceControllerService.exe install
```

### Uninstall Service
```bash
DeviceControllerService.exe uninstall
```

## IPC Contract

The service communicates with Flutter clients via Windows Named Pipes using JSON messages. See `docs/cursor/IPC_CONTRACT.md` for the complete protocol specification.

### Key Features
- **Idempotent Commands**: Every command includes a `commandId`. Duplicate `commandId` returns cached response.
- **Event Stream**: Asynchronous events for state changes
- **State Snapshots**: Clients can query current device states
- **Protocol Versioning**: All messages include `protocolVersion` for compatibility

## Development Guidelines

See `docs/cursor/DEVICE_SERVICE_PLAYBOOK.md` for detailed development rules and architecture guidelines.

### Key Principles
- **State-First Design**: Devices are modeled as state machines. Commands trigger state transitions, not direct outcomes.
- **No UI/Business Logic**: Service is a hardware control platform only.
- **Recovery Mandatory**: Automatic reconnect, backoff, hung detection.
- **Testable**: Core logic must be testable without real hardware.

## Project Structure

```
.
├── include/              # Header files
│   ├── device_abstraction/  # ICamera, IPrinter, IPaymentTerminal
│   ├── ipc/                 # IPC message types, Named Pipe server
│   ├── service_core/        # DeviceOrchestrator, RecoveryManager
│   ├── logging/             # Logger
│   └── common/              # Common utilities
├── src/                 # Implementation files
│   ├── main/            # Service entry point
│   ├── service_core/
│   ├── device_abstraction/
│   ├── ipc/
│   ├── logging/
│   └── common/
├── tests/               # Test files
├── docs/                # Documentation
└── CMakeLists.txt       # Build configuration
```

## Next Steps

1. Implement vendor adapters for specific hardware SDKs
2. Add fake device implementations for testing
3. Implement event broadcasting to connected clients
4. Add comprehensive unit tests
5. Add integration tests
