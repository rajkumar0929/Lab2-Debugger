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



#define MAX_BREAKPOINTS 32

typedef struct {
    unsigned long addr;
    unsigned char saved_byte;
    int enabled;
} breakpoint_t;

static breakpoint_t breakpoints[MAX_BREAKPOINTS];
static int breakpoint_count = 0;
static int child_running = 1;

static breakpoint_t *find_breakpoint(unsigned long addr) {
    for (int i = 0; i < breakpoint_count; i++) {
        if (breakpoints[i].addr == addr && breakpoints[i].enabled) {
            return &breakpoints[i];
        }
    }
    return NULL;
}

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
    if (breakpoint_count >= MAX_BREAKPOINTS) {
        printf("Maximum breakpoints reached\n");
        return;
    }

    if (find_breakpoint(addr)) {
        printf("Breakpoint already exists at 0x%lx\n", addr);
        return;
    }

    long data = ptrace(PTRACE_PEEKTEXT, pid, (void *)addr, NULL);
    if (data == -1) {
        perror("ptrace(PEEKTEXT)");
        return;
    }

    breakpoint_t *bp = &breakpoints[breakpoint_count++];
    bp->addr = addr;
    bp->saved_byte = (unsigned char)(data & 0xff);
    bp->enabled = 1;

    long data_with_int3 = (data & ~0xff) | 0xcc;
    ptrace(PTRACE_POKETEXT, pid, (void *)addr, (void *)data_with_int3);

    printf("Breakpoint %d set at 0x%lx\n", breakpoint_count - 1, addr);
}

static breakpoint_t *get_hit_breakpoint(pid_t pid) {
    struct user_regs_struct regs;
    ptrace(PTRACE_GETREGS, pid, NULL, &regs);

    unsigned long hit_addr = regs.rip - 1;
    return find_breakpoint(hit_addr);
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

static void restore_breakpoint(pid_t pid, breakpoint_t *bp) {
    long data = ptrace(PTRACE_PEEKTEXT, pid, (void *)bp->addr, NULL);
    long restored = (data & ~0xff) | bp->saved_byte;
    ptrace(PTRACE_POKETEXT, pid, (void *)bp->addr, (void *)restored);
}

static void reinsert_breakpoint(pid_t pid, breakpoint_t *bp) {
    long data = ptrace(PTRACE_PEEKTEXT, pid, (void *)bp->addr, NULL);
    long data_with_int3 = (data & ~0xff) | 0xcc;
    ptrace(PTRACE_POKETEXT, pid, (void *)bp->addr, (void *)data_with_int3);
}

static void cleanup_and_detach(pid_t pid) {

    if (child_running) {
        /* Restore ALL breakpoints */
        for (int i = 0; i < breakpoint_count; i++) {
            if (breakpoints[i].enabled) {
                restore_breakpoint(pid, &breakpoints[i]);
            }
        }

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

            /* Ignore SIGWINCH (window resize signal) */
            if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGWINCH) {
                /* Resume execution automatically */
                ptrace(PTRACE_CONT, pid, NULL, NULL);
                continue;
            }

            if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP) {
                breakpoint_t *bp = get_hit_breakpoint(pid);
                if (bp) {
                    printf("Breakpoint hit at 0x%lx\n", bp->addr);

                    restore_breakpoint(pid, bp);
                    rewind_rip(pid);
                    single_step(pid);
                    reinsert_breakpoint(pid, bp);
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
        else if (strncmp(cmd, "break", 5) == 0) {
            unsigned long addr;
            if (sscanf(cmd + 5, "%lx", &addr) == 1) {
                insert_breakpoint(pid, addr);
            } else {
                printf("Usage: break <hex-address>\n");
            }
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


        /* Continue execution */
        if (ptrace(PTRACE_CONT, child_pid, NULL, NULL) == -1) {
            perror("ptrace(CONT)");
            exit(1);
        }

        /* Wait for program to exit */
        waitpid(child_pid, &status, 0);
        if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP) {
            breakpoint_t *bp = get_hit_breakpoint(child_pid);
            if (bp) {
                printf("Breakpoint hit at 0x%lx\n", bp->addr);

                restore_breakpoint(child_pid, bp);
                rewind_rip(child_pid);
                single_step(child_pid);
                reinsert_breakpoint(child_pid, bp);

                /* ENTER interactive loop instead of exiting */
                command_loop(child_pid);
            }
        }
    }

    else {
        perror("fork");
        exit(1);
    }
}
