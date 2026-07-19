# KyuzenOS Build & Testing Rules

Part of the KyuzenOS Development Rules. See `RULES.md` for general principles.

---

# 1. Build Verification

Before considering any task complete:

1. Open PowerShell.

2. Initialize the Clang environment.

Example:

clang

(or any project-specific environment required)

3. Build the project.

Example:

make

or

make run

4. Fix every compilation error.

5. Fix every warning whenever possible.

Never consider work finished without a successful build.

---

# 2. Testing

New features should be tested.

Bug fixes should include regression verification.

Never assume code works.

Verify it.
