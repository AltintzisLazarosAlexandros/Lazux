/*
 * plic.h - Platform-Level Interrupt Controller (SiFive PLIC)
 *
 * The PLIC is the funnel every external device interrupt passes through on the
 * QEMU virt machine. Devices raise a numbered IRQ into the PLIC; the PLIC picks
 * the highest-priority one that is enabled for the requesting context and
 * asserts the supervisor external interrupt (scause 9) at the hart.
 *
 * "Context" is the PLIC's word for a (hart, privilege level) pair. On QEMU virt
 * with one hart, context 0 is hart0 M-mode and context 1 is hart0 S-mode. Lazux
 * runs in S-mode, so everything here targets context 1.
 */
#pragma once
#include "platform.h"

/*
 * Our context: hart 0 in S-mode. Contexts come in (M-mode, S-mode) pairs per
 * hart, so the S-mode context of hart h is 2*h + 1.
 */
#define PLIC_SCONTEXT 1

/*
 * Register addresses. Each macro yields an address (a number), not a pointer:
 * cast it to 'volatile uint32_t *' at the point of use. Every PLIC register is
 * 32 bits wide, and 'volatile' stops the compiler from dropping or reordering
 * accesses (a claim read has side effects).
 *
 * Layout: PLIC_BASE + region offset + index * stride
 *
 *   PLIC_PRIORITY(irq)    per source, 0 = never fire          stride 4
 *   PLIC_SENABLE(ctx)     per context, bit array of sources   stride 0x80
 *                         -> points at the FIRST enable word (IRQs 0-31); IRQ n
 *                            is bit n % 32 of word n / 32. Word 0 is enough while
 *                            the only source is the UART (IRQ 10).
 *   PLIC_STHRESHOLD(ctx)  per context, only priority > threshold gets through
 *                                                              stride 0x1000
 *   PLIC_SCLAIM(ctx)      per context, read = claim (returns IRQ, 0 if none),
 *                         write the same IRQ back = complete    stride 0x1000
 */
#define PLIC_PRIORITY(irq)    (PLIC_BASE + 4 * (irq))
#define PLIC_SENABLE(ctx)     (PLIC_BASE + 0x2000 + 0x80 * (ctx))
#define PLIC_STHRESHOLD(ctx)  (PLIC_BASE + 0x200000 + 0x1000 * (ctx))
#define PLIC_SCLAIM(ctx)      (PLIC_BASE + 0x200004 + 0x1000 * (ctx))

/*
 * plic_init() - Enable the devices Lazux cares about.
 *
 * Gives each IRQ a non-zero priority (priority 0 means "never fire"), enables
 * it for our context, and drops the context threshold to 0 so any priority
 * above zero gets through.
 */
void plic_init(void);

/*
 * plic_dispatch() - Service one pending external interrupt.
 *
 * Claims the highest-priority pending IRQ, routes it to the matching driver,
 * then completes it so the PLIC will deliver that IRQ again.
 *
 * Called from two places: the scause-9 arm of trap_handler(), and the idle
 * loop in schedule(), which has to dispatch by hand because it never takes the
 * trap. Both paths share this one copy of the routing logic.
 */
void plic_dispatch(void);
