/*
 * user/main.c - User-Space Test Program
 * 
 * Compiled as freestanding ELF64 binary (not linked with glibc).
 * Runs in U-mode (user privilege level) inside isolated virtual address space.
 * Cannot directly access kernel memory or hardware.
 * Must use syscalls (ecall instruction) to request kernel services.
 */
#include "lazux.h"
/*
 * main() - User program entry point
 * 
 * Entry point for this user-space test program.
 * Called by start.S bootstrap code (_start).
 * 
 * This program simply outputs a greeting message using the syscall interface.
 * Demonstrates:
 * - Compilation as freestanding C code (no libc)
 * - Syscall ABI (a7 = function ID, a0-a6 = arguments)
 * - User-to-kernel transition via ecall
 * - Process termination (implicit return to _start)
 * 
 * returns: 0 (exit code, unused in current implementation)
 */
int main() {
    puts("Lazux Shell Started!\n");
    int pid = fork();
    
    if (pid == 0) {
        exec(1);
    } else {
        int dead_child = wait();
        
        puts("Lazux Shell: Child process with PID ");
        putint(dead_child);
        puts(" has finished.\nShell is sleeping forever.\n");
        
    }
    return 0;
}
