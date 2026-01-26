You are working on the Device Controller Service (C++ Windows Service).

SOURCE OF TRUTH:

- docs/cursor/DEVICE_SERVICE_PLAYBOOK.md
- docs/cursor/IPC_CONTRACT.md

MANDATORY:

- Treat that document as the highest-priority specification.
- Do NOT introduce UI, session, or business logic.
- Preserve strict architectural layering.
- Follow state-first design and idempotent IPC rules.
- Implement recovery (reconnect, backoff, hung detection) explicitly.

If this request conflicts with the playbook,
the playbook ALWAYS wins.

If the playbook is not accessible,
STOP and ask the user to attach it.

Now proceed with the user's request.
