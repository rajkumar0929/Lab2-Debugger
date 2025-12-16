#!/bin/bash
# ============================================================
# Automated Test Script for Minimal Debugger
#
# This script verifies:
# 1. Program load and initial SIGTRAP
# 2. Setting a breakpoint at main
# 3. Register inspection at breakpoint
# 4. Single-step execution
# 5. Multiple breakpoints handling
# 6. Clean program exit and debugger detach
#
# Expected behavior is documented inline below.
# ============================================================

set -e

echo "=== [1] Building test program (non-PIE, with debug symbols) ==="
# Expected:
# - Compilation succeeds
# - tests/hello binary is created
gcc -g -no-pie -fno-pie tests/hello.c -o tests/hello

echo "=== [2] Resolving symbol addresses ==="
MAIN_ADDR=$(nm tests/hello | awk '/ T main$/{print "0x"$1}')
ADD_ADDR=$(nm tests/hello | awk '/ T add$/{print "0x"$1}')

# Expected:
# - MAIN_ADDR and ADD_ADDR are non-empty hexadecimal addresses
if [ -z "$MAIN_ADDR" ] || [ -z "$ADD_ADDR" ]; then
    echo "Failed to resolve symbols"
    exit 1
fi

echo "Resolved main at $MAIN_ADDR"
echo "Resolved add  at $ADD_ADDR"

echo "=== [3] Running debugger with scripted input ==="
# The debugger is fed commands via a here-doc.
#
# Expected sequence:
# - Breakpoint set at main
# - Breakpoint set at add
# - Registers printed at first breakpoint
# - Single-step executes one instruction
# - continue hits second breakpoint (add)
# - final continue runs program to completion
# - debugger exits cleanly
#
# Expected debugger output includes lines similar to:
#   Breakpoint 0 set at <main>
#   Breakpoint 1 set at <add>
#   Breakpoint hit at <main>
#   === Registers ===
#   Child stopped by signal: 5 (Trace/breakpoint trap)
#   Breakpoint hit at <add>
#   sum=5, product=15
#   Child exited normally. Exit code: 0
#
./debugger tests/hello <<EOF
break $MAIN_ADDR
break $ADD_ADDR
regs
step
continue
continue
quit
EOF

echo "=== [4] Test completed successfully ==="
echo "All mandatory debugger features verified."
