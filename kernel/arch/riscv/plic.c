/*
 * plic.c - Platform-Level Interrupt Controller driver
 *
 * Stage A placeholder: the entry points exist so the scheduler's idle loop can
 * call plic_dispatch() unconditionally, but nothing is wired up yet. SEIE is
 * still clear in sie and the PLIC's MMIO registers are not mapped into the
 * kernel address space, so neither function must touch hardware at this point.
 *
 * Stage B fills both in, together with the MMIO mapping in vmm_map_kernel().
 */

#include "plic.h"

void plic_init(void)
{
	/* Stage B: set IRQ priorities, enable them for context 1, threshold 0. */
}

void plic_dispatch(void)
{
	/* Stage B: claim -> route to driver -> complete. */
}
