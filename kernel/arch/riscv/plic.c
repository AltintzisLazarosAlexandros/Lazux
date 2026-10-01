/*
 * plic.c - Platform-Level Interrupt Controller driver
 *
 * Configures the PLIC so that UART0 (IRQ 10) can interrupt hart 0 in S-mode,
 * and routes each external interrupt to the driver that owns it.
 *
 * Register addresses come from the macros in plic.h (built on PLIC_BASE from
 * platform.h). Both windows were identity-mapped into every page table by
 * vmm_map_kernel(), so these accesses are safe once the MMU is on.
 *
 * Status (Stage B step 4): live. kmain enables SEIE in sie and the UART's
 * receive interrupt, so every received byte travels
 *   UART -> PLIC -> sip.SEIP -> trap_handler (scause 9) -> plic_dispatch()
 *        -> uart_intr() -> RX ring (read later by uart_getc / SYS_READ)
 */

#include "plic.h"
#include "sbi.h"
#include "uart.h"
#include <stdint.h>

/*
 * plic_init() - Let UART0 interrupts through to our context.
 *
 * Must run after the MMU is on (the PLIC is reached through its identity
 * mapping) and before interrupts are enabled.
 */
void plic_init(void)
{
	/*
	 * Priority 1 for the UART. Priority 0 means "never fire", and that is the
	 * reset value, so without this write the UART is silently ignored no matter
	 * what else is configured.
	 */
	*(volatile uint32_t *)PLIC_PRIORITY(UART0_IRQ) = 1;

	/*
	 * Listen to the UART in our context. '|=' sets only bit UART0_IRQ and leaves
	 * any other enabled sources in the same enable word alone.
	 */
	*(volatile uint32_t *)PLIC_SENABLE(PLIC_SCONTEXT) |= 1U << UART0_IRQ;

	/* Threshold 0: any source with priority > 0 gets through. */
	*(volatile uint32_t *)PLIC_STHRESHOLD(PLIC_SCONTEXT) = 0;
}

/*
 * plic_dispatch() - Claim one pending external interrupt, route it, complete it.
 *
 * Reached from the scause-9 branch of trap_handler() (step 3) and from
 * idle_service() in the scheduler, which calls it by hand when sip.SEIP is set.
 */
void plic_dispatch(void)
{
	/*
	 * Claim: reading the claim register returns the highest-priority pending
	 * IRQ for our context and marks it "in service", so the PLIC won't deliver
	 * it again until we complete it.
	 */
	uint32_t irq = *(volatile uint32_t *)PLIC_SCLAIM(PLIC_SCONTEXT);

	/*
	 * 0 = nothing pending. Normal, not an error: the idle loop may call us after
	 * the interrupt was already handled. Return without completing - there is
	 * nothing to complete.
	 */
	if (irq == 0) return;

	/* Route: hand the interrupt to the driver that owns this source. */
	switch (irq) {
	case UART0_IRQ:
		/*
		 * The UART driver drains every waiting byte into its RX ring, echoes
		 * it and wakes blocked readers. Draining is what lowers the UART's
		 * line, so it must happen BEFORE the complete below - otherwise the
		 * PLIC would re-deliver the same interrupt forever.
		 */
		uart_intr();
		break;
	default:
		/* A source we never enabled. Report it; don't halt the kernel. */
		sbi_puts("[plic] unexpected IRQ 0x");
		puthex(irq);
		sbi_puts("\n");
		break;
	}

	/*
	 * Complete: write the same IRQ number back to the same register. Until this
	 * write the PLIC will not deliver this source again. It comes after the
	 * driver ran, so the device has already lowered its line.
	 */
	*(volatile uint32_t *)PLIC_SCLAIM(PLIC_SCONTEXT) = irq;
}
