# KyuzenOS Style Guide

Part of the KyuzenOS Development Rules. See `RULES.md` for general principles.

---

# 1. Clean Code

Every source file must follow clean code principles.

Required:

- Small and focused functions.
- Descriptive names.
- No duplicated logic.
- Single Responsibility Principle.
- Consistent formatting.
- Remove dead code.
- Remove commented-out code.
- No unnecessary macros.
- No magic numbers.
- Prefer constexpr/static const over macros whenever possible.
- Minimize global variables.

Never write code that only "works".

Write code that is maintainable.

---

# 2. Comments

Comments should explain WHY.

Never explain WHAT.

Bad:

// increment i

Good:

// Skip reserved kernel pages to prevent allocator corruption.

---

# 3. Naming

Use descriptive names.

Good:

PhysicalMemoryManager

Bad:

PM

Good:

InitializeScheduler()

Bad:

InitSched()

Abbreviations should only be used when already established.

Examples:

PCI
DMA
IRQ
TLB
VMM
PMM

---

# 4. Performance

Optimize only when necessary.

Never sacrifice readability for micro-optimizations.

Avoid:

- unnecessary allocations
- repeated page table walks
- repeated locking
- unnecessary memory copies
