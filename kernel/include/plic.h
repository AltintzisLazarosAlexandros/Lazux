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
