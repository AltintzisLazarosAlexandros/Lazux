/*
 * platform.h - QEMU virt board memory map
 *
 * Where the devices Lazux talks to live in physical memory, and how big their
 * register windows are. This header answers "where is the device" only; how to
 * drive a device (register offsets, bit meanings) belongs in that device's own
 * header (plic.h, and later uart.h).
 *
 * Values come from the machine's device tree - dump it with:
 *   qemu-system-riscv64 -machine virt,dumpdtb=virt.dtb
 *   dtc -I dtb -O dts virt.dtb
 * and read each node's 'reg = <base size>'.
 *
 * All MMIO windows are identity-mapped by vmm_map_kernel() as R+W: never X
 * (device memory is not code) and never U (user space must not touch hardware).
 */

#pragma once

/*
 * UART0 - NS16550A serial port, the same console OpenSBI prints through.
 * Its 8 one-byte registers fit in the 0x100 window the device tree declares;
 * map_identity_range() rounds that up to a single 4KB page.
 */
#define UART0_BASE 0x10000000UL
#define UART0_SIZE 0x100UL
#define UART0_IRQ  10          /* PLIC interrupt source number for UART0 */

/*
 * PLIC - Platform-Level Interrupt Controller.
 *
 * QEMU's full PLIC window is sized for the maximum number of harts, but Lazux
 * runs on one hart and only uses its S-mode context (context 1). The highest
 * register touched is context 1's claim/complete at base + 0x201004, so the
 * window ends at the next page boundary:
 *
 *   0x200000 (context base) + 2 contexts * 0x1000 (stride) = 0x202000
 *
 * If Lazux ever runs on more than one hart, this size must grow to cover the
 * extra contexts.
 */
#define PLIC_BASE 0x0C000000UL
#define PLIC_SIZE 0x202000UL
