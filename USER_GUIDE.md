# Minimal Debugger — User Guide

## 1. Introduction

This project implements a **minimal command-line debugger** for Linux ELF binaries using the `ptrace` system call.  
The debugger allows users to load a program, set software breakpoints, inspect CPU registers, control execution, and safely detach from the target process.

**No breakpoints are set automatically**. All debugging actions are explicitly controlled by the user.

The project is intended for **educational purposes**, demonstrating the core mechanisms used by real debuggers such as GDB.

---

## 2. System Requirements

- Linux (native Linux or **WSL2** on Windows)
- x86_64 architecture
- GCC compiler
- `ptrace` enabled (default on most Linux systems)

> ⚠️ Only Linux ELF binaries are supported.  
> Windows executables (`.exe`) are **not supported**.

---

## 3. Building the Project

From the project root directory, run:

```bash
make
```

This builds:
- `debugger` — the debugger executable
- `tests/hello` — a sample target program

To clean all build artifacts:

```bash
make clean
```

---

## 4. Running the Debugger

### Syntax

```bash
./debugger <program>
```

### Example

```bash
./debugger ./tests/hello
```

On startup, the debugger:
1. Forks a child process
2. Attaches using `ptrace`
3. Stops execution at the initial `SIGTRAP`
4. Displays CPU register values

If no breakpoints are set, the program continues execution normally after this initial stop.

---

## 5. Debugger Prompt

The debugger prompt appears **only when execution is paused**, such as:
- After hitting a breakpoint
- After executing a single-step instruction

```text
dbg>
```

---

## 6. Supported Commands

### 6.1 `break <hex-address>`

Sets a **software breakpoint** at the specified virtual address.

- Address must be provided in **hexadecimal**
- Multiple breakpoints are supported
- Duplicate breakpoints are rejected

```text
dbg> break 0x401171
Breakpoint 0 set at 0x401171
```

---

### 6.2 `continue` or `c`

Resumes execution of the target program until:
- A breakpoint is hit
- The program exits
- The program is stopped by a signal

Signals such as `SIGWINCH` (window resize) are automatically ignored and execution resumes transparently.

```text
dbg> continue
Breakpoint hit at 0x401171
```

---

### 6.3 `step` or `s`

Executes **exactly one machine instruction** and then stops.

```text
dbg> step
Child stopped by signal: 5 (Trace/breakpoint trap)
```

---

### 6.4 `regs`

Displays the current values of key CPU registers.

Registers displayed include:
- RIP (Instruction Pointer)
- RSP (Stack Pointer)
- RBP (Base Pointer)
- RAX, RBX, RCX, RDX

---

### 6.5 `quit`

Safely exits the debugger.

Before exiting:
- All active breakpoints are restored
- The debugger detaches from the target process (if still running)
- No modified instructions remain in memory

```text
dbg> quit
Exiting debugger.
```

---

## 7. Example Debugging Session

```text
$ ./debugger ./tests/hello
Child stopped by signal: 5 (Trace/breakpoint trap)

dbg> break 0x401171
Breakpoint 0 set at 0x401171

dbg> continue
Breakpoint hit at 0x401171

dbg> regs
...

dbg> step
Child stopped by signal: 5 (Trace/breakpoint trap)

dbg> continue
sum=5, product=15
Child exited normally. Exit code: 0

dbg> quit
Exiting debugger.
```

---

## 8. Notes and Limitations

- Breakpoints must be specified using **absolute virtual addresses**
- Position Independent Executables (PIE) should be disabled for predictable addresses
- Symbolic breakpoints (e.g., `break main`) are **not supported**
- Multi-threaded programs are **not supported**
- Some signals (e.g., `SIGWINCH`) are handled internally and may not be displayed

---

## 9. Summary

This debugger demonstrates:
- Process tracing using `ptrace`
- Software breakpoints using `INT3 (0xCC)`
- Instruction pointer manipulation
- Single-step execution
- Multiple breakpoint support
- Safe cleanup and detachment

