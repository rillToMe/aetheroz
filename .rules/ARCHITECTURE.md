# KyuzenOS Architecture Rules

Part of the KyuzenOS Development Rules. See `RULES.md` for general principles.

---

# 1. Project Architecture

Always respect the existing project structure.

Never move files unless absolutely necessary.

Never rename public APIs without updating every reference.

Never create duplicate systems.

Always extend existing systems when possible.

---

# 2. Kernel APIs

Kernel APIs should be:

- small
- predictable
- documented

Avoid huge functions.

Prefer reusable interfaces.

---

# 3. Drivers

Drivers should never contain unrelated logic.

Each driver should:

- initialize
- shutdown
- reset
- validate hardware
- report failures

Hardware-specific code stays inside drivers.

---

# 4. Error Handling

Never ignore return values.

Always validate:

- pointers
- allocations
- hardware initialization
- file operations
- memory mapping

Kernel code must fail safely.

---

# 5. Memory Safety

Always:

- free allocated memory
- validate pointers
- prevent buffer overflow
- prevent integer overflow
- check page alignment
- check physical alignment when required

Memory leaks are bugs.

---

# 6. Synchronization

Shared resources must be protected.

Never introduce race conditions.

Always use the existing synchronization primitives.

Never hold locks longer than necessary.

---

# 7. Logging

Use kernel logging consistently.

Every important subsystem should report:

- initialization
- warnings
- errors

Do not spam logs.

---

# 8. Refactoring Rules

Do not refactor unrelated code.

If touching one subsystem:

- keep behavior identical
- preserve API compatibility
- avoid unnecessary formatting changes

Refactoring must improve maintainability.
