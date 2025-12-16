#include "debugger.h"

#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>   // for strsignal
#include <sys/user.h>


//0000000000001149

static unsigned long breakpoint_addr = 0;  //where the breakpoint is
static unsigned char saved_byte = 0;       //what original byte we overwrote
static int breakpoint_active = 0;
static int child_running = 1;


static void print_wait_status(int status) {
    if (WIFSTOPPED(status)) {
        int sig = WSTOPSIG(status);
        printf("Child stopped by signal: %d (%s)\n",
               sig, strsignal(sig));
    }
    else if (WIFEXITED(status)) {
        printf("Child exited normally. Exit code: %d\n",
               WEXITSTATUS(status));
        child_running = 0;
    }
    else if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        printf("Child terminated by signal: %d (%s)\n",
               sig, strsignal(sig));
        child_running = 0;
    }
    else {
        printf("Unknown child status\n");
    }
}

static void print_registers(pid_t pid) {
    struct user_regs_struct regs;

    if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) == -1) {
        perror("ptrace(GETREGS)");
        return;
    }

    printf("=== Registers ===\n");
    printf("RIP: 0x%llx\n", regs.rip);
    printf("RSP: 0x%llx\n", regs.rsp);
    printf("RBP: 0x%llx\n", regs.rbp);
    printf("RAX: 0x%llx\n", regs.rax);
    printf("RBX: 0x%llx\n", regs.rbx);
    printf("RCX: 0x%llx\n", regs.rcx);
    printf("RDX: 0x%llx\n", regs.rdx);
    printf("=================\n");
}

static void insert_breakpoint(pid_t pid, unsigned long addr) {
    long data = ptrace(PTRACE_PEEKTEXT, pid, (void *)addr, NULL);
    if (data == -1) {
        perror("ptrace(PEEKTEXT)");
        exit(1);
    }

    saved_byte = (unsigned char)(data & 0xff);
    long data_with_int3 = (data & ~0xff) | 0xcc;

    if (ptrace(PTRACE_POKETEXT, pid, (void *)addr, (void *)data_with_int3) == -1) {
        perror("ptrace(POKETEXT)");
        exit(1);
    }

    breakpoint_addr = addr;
    printf("Breakpoint inserted at 0x%lx\n", addr);
    breakpoint_active = 1;
}

static int is_breakpoint_hit(pid_t pid) {
    struct user_regs_struct regs;
    ptrace(PTRACE_GETREGS, pid, NULL, &regs);

    if (regs.rip - 1 == breakpoint_addr) {
        printf("Breakpoint hit at 0x%lx\n", breakpoint_addr);
        return 1;
    }
    return 0;
}

static void restore_breakpoint(pid_t pid) {
    long data = ptrace(PTRACE_PEEKTEXT, pid, (void *)breakpoint_addr, NULL);
    if (data == -1) {
        perror("ptrace(PEEKTEXT)");
        exit(1);
    }

    long restored = (data & ~0xff) | saved_byte;

    if (ptrace(PTRACE_POKETEXT, pid, (void *)breakpoint_addr, (void *)restored) == -1) {
        perror("ptrace(POKETEXT)");
        exit(1);
    }
}

static void rewind_rip(pid_t pid) {
    struct user_regs_struct regs;

    ptrace(PTRACE_GETREGS, pid, NULL, &regs);
    regs.rip -= 1;
    ptrace(PTRACE_SETREGS, pid, NULL, &regs);
}

static void single_step(pid_t pid) {
    ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL);
    waitpid(pid, NULL, 0);
}

static void reinsert_breakpoint(pid_t pid) {
    long data = ptrace(PTRACE_PEEKTEXT, pid, (void *)breakpoint_addr, NULL);
    long data_with_int3 = (data & ~0xff) | 0xcc;
    ptrace(PTRACE_POKETEXT, pid, (void *)breakpoint_addr, (void *)data_with_int3);
}

static void cleanup_and_detach(pid_t pid) {

    /* Restore breakpoint ONLY if child is still running */
    if (child_running && breakpoint_active) {
        restore_breakpoint(pid);
        breakpoint_active = 0;
        printf("Breakpoint restored before detach.\n");
    }

    /* Detach ONLY if child is still running */
    if (child_running) {
        if (ptrace(PTRACE_DETACH, pid, NULL, NULL) == -1) {
            perror("ptrace(DETACH)");
        } else {
            printf("Detached cleanly from child.\n");
        }
    }
}


static void command_loop(pid_t pid) {
    char cmd[64];

    while (1) {
        printf("dbg> ");
        fflush(stdout);

        if (!fgets(cmd, sizeof(cmd), stdin))
            break;

        if (strncmp(cmd, "continue", 8) == 0 || strncmp(cmd, "c", 1) == 0) {
            ptrace(PTRACE_CONT, pid, NULL, NULL);
            int status;
            waitpid(pid, &status, 0);
            print_wait_status(status);

            if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP) {
                if (is_breakpoint_hit(pid)) {
                    restore_breakpoint(pid);
                    rewind_rip(pid);
                    single_step(pid);
                    reinsert_breakpoint(pid);
                    printf("Breakpoint hit again.\n");
                }
            }
        }
        else if (strncmp(cmd, "step", 4) == 0 || strncmp(cmd, "s", 1) == 0) {
            ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL);
            int status;
            waitpid(pid, &status, 0);
            print_wait_status(status);
        }
        else if (strncmp(cmd, "regs", 4) == 0) {
            print_registers(pid);
        }
        else if (strncmp(cmd, "quit", 4) == 0) {
            cleanup_and_detach(pid);
            printf("Exiting debugger.\n");
            break;
        }
        else {
            printf("Unknown command\n");
        }
    }
}



void debugger_start(char *program) {
    pid_t child_pid = fork();

    if (child_pid == 0) {
        /* ---- CHILD ---- */
        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) == -1) {
            perror("ptrace(TRACEME)");
            exit(1);
        }

        execl(program, program, NULL);
        perror("execl");
        exit(1);
    }
    else if (child_pid > 0) {
        int status;

        /* Wait for initial SIGTRAP */
        waitpid(child_pid, &status, 0);
        print_wait_status(status);

        /* Print registers at stop */
        print_registers(child_pid);

        unsigned long main_addr = 0x401136; 
        insert_breakpoint(child_pid, main_addr);


        /* Continue execution */
        if (ptrace(PTRACE_CONT, child_pid, NULL, NULL) == -1) {
            perror("ptrace(CONT)");
            exit(1);
        }

        /* Wait for program to exit */
        waitpid(child_pid, &status, 0);
        if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP) {
            if (is_breakpoint_hit(child_pid)) {

                /* 1. Restore original instruction */
                restore_breakpoint(child_pid);

                /* 2. Fix RIP */
                rewind_rip(child_pid);

                /* 3. Execute the real instruction */
                single_step(child_pid);

                /* 4. Reinsert breakpoint */
                reinsert_breakpoint(child_pid);

                printf("Breakpoint handled correctly, execution paused.\n");
                command_loop(child_pid);
                return;
            }
        }
    }

    else {
        perror("fork");
        exit(1);
    }
}
