/*
 * entry.S - Kernel Assembly Entry Point
 * 
 * This is the very first code executed by the CPU after OpenSBI loads the kernel.
 * It performs bare-metal initialization before jumping to kmain() in C.
 * 
 * Responsibilities:
 * 1. Set up the kernel stack pointer
 * 2. Clear the .bss section (uninitialized data)
 * 3. Call kmain() to enter C code
 * 4. Hang forever if kmain returns (should not happen)
 */

	.section .text
	.globl _start

_start:
	/* Load the top of the kernel stack into sp (stack pointer)
	   The linker script defines __stack_top at a high address */
	la sp, __stack_top

	/* BSS (Block Started by Symbol) is uninitialized data that must be zeroed.
	   Load the start and end addresses from the linker script */
	la t0, __bss_start
	la t1, __bss_end

/* Loop: Zero out 8 bytes at a time until we reach __bss_end */
clear_bss:
	bgeu t0, t1, bss_done      /* If t0 >= t1, we're done; jump to bss_done */
	sd zero, 0(t0)             /* Store 64-bit zero at [t0] (8 bytes) */
	addi t0, t0, 8             /* Move t0 forward by 8 bytes */
	j clear_bss                /* Loop back to clear_bss */

bss_done:
	/* BSS is now cleared; jump to kmain() to begin kernel execution */
	call kmain

/* If kmain ever returns (it shouldn't), halt the CPU */
hang:
	wfi                        /* Wait for interrupt (puts CPU in low-power state) */
	j hang                     /* Loop forever */
