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

Code must speak for itself. Prefer clear naming and structure over comments.

Keep comments minimal. Do not comment to save tokens or padding — every comment must earn its place.

Never explain WHAT. Never comment obvious code.

Bad:

// increment i

Only comment non-obvious WHY, and keep it to one short line:

// Skip reserved kernel pages to prevent allocator corruption.

If an explanation needs more than one line, it does not belong in the code. Put it in `DOCUMENTATION.md` instead and, if needed, leave a short pointer comment referencing it.

**Above all: keep comments short and to the point.** No long sentences, no multi-line comments, no elaboration inside the code. One short line, straight to the point, or no comment at all.

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