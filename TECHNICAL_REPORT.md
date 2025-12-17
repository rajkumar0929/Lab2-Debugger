# Minimal Debugger — Technical Report

## 1. Introduction

Debuggers are fundamental tools in systems programming, enabling controlled execution, inspection of program state, and diagnosis of software faults.  
This project implements a **minimal command-line debugger** for Linux ELF binaries using the `ptrace` system call.

The objective of the assignment is to understand and implement the *core internal mechanisms* of a debugger rather than building a feature-rich tool. The final implementation supports user-controlled breakpoints, register inspection, single-step execution, controlled continuation, and safe cleanup.

Unlike earlier iterations, the final debugger design **does not set any breakpoints automatically**. All debugging actions are explicitly initiated by the user, resulting in predictable and transparent behavior.

---

## 2. System Architecture Overview

### 2.1 Parent–Child Process Model

The debugger follows the classic **parent–child model**:

- The debugger process acts as the *tracer*.
- The target program (debuggee) acts as the *tracee*.

Execution flow:
1. The debugger creates a child process using `fork()`.
2. The child calls `ptrace(PTRACE_TRACEME)` to declare itself traceable.
3. The child replaces its image using  `execl()`.
4. After `execve()`, the kernel delivers an initial `SIGTRAP`, stopping the child.
5. The parent gains control and begins debugging.

This initial stop ensures that the debugger can inspect registers or set breakpoints before the program executes user instructions.

---

### 2.2 Event Synchronization with `waitpid`

The debugger synchronizes with the child process using `waitpid()`.  
The returned status value is decoded using standard macros:

- `WIFSTOPPED(status)` — child stopped by a signal
- `WSTOPSIG(status)` — signal that caused the stop
- `WIFEXITED(status)` — normal program termination
- `WEXITSTATUS(status)` — exit code
- `WIFSIGNALED(status)` — termination due to signal

This mechanism allows the debugger to distinguish between breakpoint hits, single-step traps, normal exits, and external signals.

---

## 3. Software Breakpoints

### 3.1 Breakpoint Implementation (INT3)

The debugger implements **software breakpoints** using the x86-64 `INT3` instruction (`0xCC`).

To insert a breakpoint at address `A`:
1. The original instruction word at `A` is read using `ptrace(PTRACE_PEEKTEXT)`.
2. The least significant byte is saved.
3. The byte is replaced with `0xCC`.
4. The modified word is written back using `ptrace(PTRACE_POKETEXT)`.

When the CPU executes `INT3`, it generates a `SIGTRAP`, transferring control to the debugger.

---

### 3.2 Instruction Pointer Correction

After executing `INT3`, the CPU advances the instruction pointer (`RIP`) by one byte.  
Therefore, upon receiving `SIGTRAP`, the debugger must:

- Restore the original instruction byte
- Decrement `RIP` by 1

This correction ensures that the original instruction executes correctly and is a critical detail in breakpoint handling.

---

### 3.3 Restore–Rewind–Step–Reinsert Cycle

To correctly handle breakpoints, the debugger performs the following sequence:

1. Restore the original instruction byte
2. Rewind `RIP` by one byte
3. Execute exactly one instruction using `PTRACE_SINGLESTEP`
4. Reinsert the breakpoint (`INT3`)

This cycle ensures correctness and allows breakpoints to remain active across multiple hits.

---

## 4. Multiple Breakpoint Management

The debugger supports **multiple breakpoints** using a fixed-size breakpoint table.

Each breakpoint entry contains:
- Breakpoint address
- Saved original instruction byte
- Enabled flag

A linear lookup is used to:
- Detect duplicate breakpoints
- Identify which breakpoint was hit by comparing `(RIP - 1)` with stored addresses

This design balances simplicity and functionality.

---

## 5. Register Inspection

The debugger allows inspection of CPU registers using:

```c
ptrace(PTRACE_GETREGS, pid, NULL, &regs);
```

Displayed registers include:
- Instruction pointer (`RIP`)
- Stack pointer (`RSP`)
- Base pointer (`RBP`)
- General-purpose registers (`RAX`, `RBX`, `RCX`, `RDX`)

Register inspection is especially useful at breakpoint hits and during single-step execution.

---

## 6. Execution Control

### 6.1 Continue Execution

Execution is resumed using:

```c
ptrace(PTRACE_CONT, pid, NULL, NULL);
```

The debugger then waits for the next stop event, which may occur due to a breakpoint, signal, or program termination.

Signals such as `SIGWINCH` (window resize) are **ignored automatically** during continuation, preventing unnecessary interruptions during debugging sessions.

---

### 6.2 Single-Step Execution

Single-instruction execution is supported using:

```c
ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL);
```

After executing one instruction, the kernel stops the child and delivers `SIGTRAP`, allowing instruction-level control.

---

## 7. Interactive Command Interface

The debugger provides a minimal interactive interface with the prompt:

```text
dbg>
```

Supported commands include:

- `break <address>` — set a breakpoint
- `continue` / `c` — resume execution
- `step` / `s` — execute one instruction
- `regs` — display CPU registers
- `quit` — exit the debugger safely

The prompt appears **only when execution is paused**, such as after a breakpoint hit or single-step.

---

## 8. Cleanup and Safe Detachment

Correct cleanup is essential to prevent memory corruption or zombie processes.

The debugger:
- Tracks whether the child process is still running
- Restores **all active breakpoints** before detaching
- Calls `ptrace(PTRACE_DETACH)` only if the child is alive
- Avoids invalid `ptrace` calls after program termination

This design guarantees that no modified instructions remain in the target process.

---

## 9. Testing and Validation

Automated testing is provided via `tests/run_tests.sh`.  
The script verifies:

- Successful program loading
- Breakpoint insertion and duplicate detection
- Multiple breakpoint handling
- Register inspection at breakpoints
- Single-step execution
- Program completion without corruption
- Clean debugger exit

The test script dynamically resolves symbol addresses using `nm`, avoiding hardcoded values.

---

## 10. Limitations

The debugger has the following limitations:

- Only absolute-address breakpoints are supported
- Symbolic breakpoints (e.g., `break main`) are not implemented
- Position Independent Executables (PIE) must be disabled
- Multi-threaded programs are not supported
- Signal handling is minimal by design

---

## 11. Conclusion

This project successfully demonstrates the essential mechanisms of a debugger:

- Process tracing using `ptrace`
- Software breakpoints via `INT3`
- Instruction pointer manipulation
- Single-step and controlled execution
- Multiple breakpoint support
- Robust cleanup and safe detachment

The final implementation fulfills all mandatory requirements of the assignment and reflects correct, professional debugger design principles.
