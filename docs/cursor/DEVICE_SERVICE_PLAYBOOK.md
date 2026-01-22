# DEVICE_SERVICE_PLAYBOOK.md
## Device Controller Service – Cursor Instructions (C++ / Windows Service)

> This document is intended to be provided directly to Cursor.
>  
> When writing or modifying Device Controller Service code,
> Cursor MUST treat this document as the highest-priority reference.

---

## 1. Project Identity (Device Controller Service)

This project is a **C++-based Windows Service** that acts as a  
**hardware control platform for kiosk systems**.

### Core purpose
- Provide stable, reusable hardware control
- Abstract vendor-specific SDKs
- Expose device state and commands via IPC
- Survive long-term unattended operation

This service is **NOT an application** and **NOT a UI process**.

---

## 2. Absolute Non-Responsibilities

The Device Controller Service MUST NOT:

- Contain UI or UX logic
- Contain session, flow, or pricing logic
- Contain business or policy decisions
- Depend on Flutter or any client UI framework
- Generate images, layouts, or print compositions
- Assume user interaction or screen flow

Any of the above is considered a **platform violation**.

---

## 3. Technology Stack (Fixed Decisions)

These decisions are **intentionally fixed** to avoid ambiguity.

### Language & Runtime
- C++ (modern standard, e.g. C++20)
- Native Windows Service (Win32 Service API)

### Process Model
- Single Windows Service process
- Long-running background execution
- Auto-start on system boot

### IPC
- Local IPC only (no external network exposure)
- Named Pipes (JSON or protobuf-based messages)
- Command / Response + Event Stream + State Snapshot model

### Logging
- File-based logging
- Rotating logs (size or date based)
- Logs are for operators and developers, not for clients

---

## 4. Architectural Layers (Mandatory)

The service MUST maintain the following layers:

1. **Service Core**
   - Orchestrates devices
   - Owns state machines
   - Applies recovery logic
2. **Device Abstraction Layer**
   - Interfaces: ICamera, IPrinter, IPaymentTerminal
3. **Vendor Adapter Layer**
   - Wraps vendor SDKs (Canon, printer vendors, payment terminals)
   - Converts SDK behavior into clean state transitions
4. **IPC Layer**
   - Transports commands/events/snapshots
   - No business logic

### Forbidden dependencies
- Service Core → Vendor SDK (FORBIDDEN)
- IPC Layer → Vendor SDK (FORBIDDEN)
- Vendor Adapter → IPC Layer (FORBIDDEN)
- Lower layers referencing higher layers (FORBIDDEN)

---

## 5. State-First Design Rules

- Every device is modeled as a **state machine**
- Commands trigger **state transitions**, not direct outcomes
- Exceptions are converted into:
  - error states
  - error events

### Examples
- “capture” does not return an image
- “print” does not return success/failure immediately
- Results are observed through state changes and events

State is the **single source of truth**.

---

## 6. IPC Contract Rules

- Every command MUST include a unique `commandId`
- Commands MUST be idempotent
- Duplicate `commandId` MUST NOT re-execute logic
- Events:
  - may be duplicated
  - may arrive out of order
- Clients MUST rely on state snapshots for recovery

The IPC contract is defined in:
- `docs/cursor/IPC_CONTRACT.md`

This file is the **shared, immutable contract** between client and service.

---

## 7. Failure & Recovery Strategy

Failures are treated as **normal operating conditions**.

### Mandatory recovery mechanisms
- Automatic reconnect on device disconnect
- Backoff strategy for repeated failures
- Hung detection (timeouts with no progress)
- Safe device reinitialization

### Hung detection
- Every operation has a maximum allowed duration
- No progress within timeout ⇒ hung state
- Hung state triggers recovery or reset

### Restart strategy
- Service restart is a valid recovery action
- Prefer fail-fast + restart over silent deadlock
- Expose a controlled restart request for admin/ops

---

## 8. Service Lifecycle Rules

- Service must survive:
  - device unplug/replug
  - temporary power issues
  - client disconnects
- Client restarts MUST NOT require service restart
- Service restart MAY require client reconnection

The service is designed to outlive any single client session.

---

## 9. Testing Rules

- Core logic MUST be testable without real hardware
- Vendor adapters MUST be replaceable with fakes
- Tests MUST cover:
  - state transitions
  - idempotent command handling
  - recovery paths (disconnect, hung, retry)
- “Cannot test without device” is NOT acceptable

---

## 10. Multi-Client & Reuse Rules

- The service MUST NOT assume a single kiosk product
- Multiple kiosk applications may reuse this service
- Reuse across different machines and deployments is expected

Any change that reduces reusability is considered a regression.

---

## 11. Cursor Behavior Rules

Before writing code, Cursor MUST:

1. Identify which architectural layer owns the change
2. Ensure no forbidden dependency is introduced
3. Confirm state-first design is preserved
4. Verify idempotency and recovery behavior
5. Consider testability before implementation

If any rule is unclear, Cursor MUST pause and ask for clarification.

---

## 12. Final Principle

> The Device Controller Service is a **hardware control platform**,  
> not an application, not a UI, and not a business logic container.

Any design that violates this principle is incorrect.