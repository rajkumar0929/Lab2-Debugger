# Debugger Demo Walkthrough

This document provides a **step-by-step walkthrough** to demonstrate the functionality of the minimal Linux debugger implemented using `ptrace`.

⚠️ **Important behavioral note**  
The debugger **does not enter interactive mode at startup**.  
The interactive prompt (`dbg>`) appears **only when the target program is paused**, such as after hitting a breakpoint or during single-step execution.

This behavior is **intentional and by design**.

---

## 1. Prerequisites

- Linux environment (native Linux or WSL2)
- x86_64 architecture
- GCC compiler
- Project built successfully using:

```bash
make
```

---

## 2. Test Program Overview

The demo uses the test program:

```
tests/hello.c
```

This program:
- Contains multiple function calls (`add`, `multiply`)
- Has a clear and deterministic instruction flow
- Is compiled with debug symbols and PIE disabled

---

## 3. Demonstrating Normal Execution (No Breakpoints)

### Command

```bash
./debugger ./tests/hello
```

### Expected Output (abridged)

```text
Child stopped by signal: 5 (Trace/breakpoint trap)
=== Registers ===
...
sum=5, product=15
```

### Explanation

- The initial `SIGTRAP` occurs after `execve()`
- Registers are displayed
- Since **no breakpoints are set**, execution continues normally
- The program exits without entering interactive mode

This confirms that the debugger **does not interfere with execution unless instructed**.

---

## 4. Demonstrating Interactive Debugging

Interactive debugging is demonstrated using the **automated test script**, which injects commands at the correct execution points.

### Command

```bash
./tests/run_tests.sh
```

---

### What the Script Demonstrates

The script automatically performs the following steps:

1. Builds the test program with debug symbols
2. Resolves function addresses using `nm`
3. Starts the debugger
4. Sets breakpoints at:
   - `main`
   - `add`
5. Inspects registers at a breakpoint
6. Single-steps one instruction
7. Continues execution
8. Verifies normal program completion
9. Exits the debugger cleanly

---

### Expected Final Output

```text
All mandatory debugger features verified.
```

---

## 5. Manual Interactive Debugging (Optional, Detailed)

This section demonstrates **manual interactive debugging** without using the automated test script.  
Because the debugger does **not pause automatically**, interaction is possible **only after a breakpoint is hit**.

---

### 5.1 Understanding When Interaction Is Possible

When the debugger is started with:

```bash
./debugger ./tests/hello
```

The following occurs:

1. The child process stops once at startup (`SIGTRAP` after `execve`)
2. Register values are printed
3. Since **no breakpoint is set**, the program continues execution
4. The program exits normally
5. No `dbg>` prompt appears

---

### 5.2 Step 1: Find Breakpoint Addresses (Required)

The debugger accepts **only absolute hexadecimal addresses**.

In a separate terminal:

```bash
nm ./tests/hello
```

Example output:

```text
0000000000401136 T add
0000000000401150 T multiply
0000000000401171 T main
```

These addresses are stable because the program is compiled with **PIE disabled**.

---

### 5.3 Step 2: Start the Debugger

```bash
./debugger ./tests/hello
```

Expected output (abridged):

```text
Child stopped by signal: 5 (Trace/breakpoint trap)
=== Registers ===
...
```

At this stage:
- The program is **not paused at a breakpoint**
- Execution will continue unless a breakpoint is hit

---

### 5.4 Step 3: Set a Breakpoint (`break <address>`)

Set a breakpoint at `main`:

```text
dbg> break 0x401171
```

Expected output:

```text
Breakpoint 0 set at 0x401171
```

Set another breakpoint at `add`:

```text
dbg> break 0x401136
```

Duplicate breakpoints are rejected automatically.

---

### 5.5 Step 4: Continue Execution Until Breakpoint

```text
dbg> continue
```

Expected output:

```text
Breakpoint hit at 0x401171
```

The debugger now enters interactive mode:

```text
dbg>
```

---

### 5.6 Step 5: Inspect Registers

```text
dbg> regs
```

Displays CPU register state at the breakpoint.

---

### 5.7 Step 6: Single-Step Execution

```text
dbg> step
```

Expected output:

```text
Child stopped by signal: 5 (Trace/breakpoint trap)
```

---

### 5.8 Step 7: Continue Program Execution

```text
dbg> continue
```

Expected output:

```text
sum=5, product=15
Child exited normally. Exit code: 0
```

---

### 5.9 Step 8: Exit the Debugger

```text
dbg> quit
```

The debugger restores all breakpoints and detaches safely.

---

## 6. Key Design Decisions (For Explanation)

- No breakpoints are set automatically
- User explicitly controls when execution is paused
- Breakpoints use software `INT3 (0xCC)`
- Instruction pointer is rewound correctly after breakpoint hits
- All breakpoints are restored before detaching
- Signals such as `SIGWINCH` are ignored during `continue`

---

## 7. Conclusion

This demo confirms that the debugger correctly implements:

- Process tracing using `ptrace`
- Software breakpoint insertion and handling
- Register inspection
- Single-step execution
- Multiple breakpoint support
- Safe cleanup and program termination

The demonstrated behavior matches the documented design and fulfills all assignment requirements.
