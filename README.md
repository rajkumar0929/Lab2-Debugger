# Minimal Linux Debugger

This project implements a minimal command-line debugger for Linux ELF binaries using the `ptrace` system call.

The debugger supports:
- User-controlled software breakpoints
- Register inspection
- Single-step execution
- Controlled program continuation
- Safe cleanup and detachment

---

## Build

```bash
make
```

---

## Run

```bash
./debugger <program>
```

Example:

```bash
./debugger ./tests/hello
```

---

## Demo (Recommended)

The recommended way to demonstrate functionality is via the automated test script:

```bash
./tests/run_tests.sh
```

This script validates all mandatory debugger features in a deterministic and reproducible manner.

---

## Documentation

- **User Guide:** `USER_GUIDE.md`
- **Technical Report:** `TECHNICAL_REPORT.md`
- **Demo Walkthrough:** `demo.md`

---

## Notes

- Only Linux ELF binaries are supported
- PIE must be disabled for predictable addresses
- Interactive debugging is supported, but automated testing is preferred
