/*
 * uart.c - NS16550A UART driver (console input)
 *
 * Producer/consumer split around a receive ring buffer:
 *
 *   uart_intr()  - PRODUCER, interrupt time. Runs the moment bytes arrive,
 *                  moves them out of the UART's tiny FIFO into the ring, echoes
 *                  them, and wakes anyone waiting for input.
 *   uart_getc()  - CONSUMER, syscall time (SYS_READ, step 5). Takes bytes out
 *                  of the ring whenever a process asks. Never blocks.
 *
 * The ring decouples the two: keys typed while no one is reading wait in the
 * ring, and a reader that finds it empty sleeps on it until uart_intr() wakes it.
 *
 * No locking: uart_intr() and uart_getc() both run only inside the kernel, the
 * kernel never runs with interrupts enabled, and there is one hart - so they can
 * never execute at the same time.
 */

#include "uart.h"
#include "sbi.h"
#include "proc.h"
#include <stdint.h>

/*
 * Receive ring buffer, private to the driver.
 *
 * rx_head and rx_tail are free-running counters - they only ever go UP:
 *   rx_head = total bytes ever stored  (changed only by uart_intr)
 *   rx_tail = total bytes ever taken   (changed only by uart_getc)
 *
 *   bytes waiting = rx_head - rx_tail
 *   empty         = rx_head == rx_tail
 *   full          = rx_head - rx_tail == UART_RX_SIZE
 *   slot of n     = n % UART_RX_SIZE
 *
 * UART_RX_SIZE must be a power of two so that '% UART_RX_SIZE' stays correct
 * when the uint32_t counters eventually wrap around. All three variables live
 * in .bss, which entry.S zeroes, so the ring starts empty with no setup.
 *
 * &rx_buf doubles as the wait channel: readers sleep_on(&rx_buf), and
 * uart_intr() calls wakeup(&rx_buf).
 */
#define UART_RX_SIZE 128

static char rx_buf[UART_RX_SIZE];
static uint32_t rx_head;
static uint32_t rx_tail;

/*
 * uart_init() - Enable the receive interrupt (IER bit 0).
 *
 * Every UART register is 8 bits wide, hence the uint8_t access.
 */
void uart_init(void)
{
	*(volatile uint8_t *)UART_IER = UART_IER_RX;
}

/*
 * uart_intr() - Interrupt handler, called from plic_dispatch() before complete.
 */
void uart_intr(void)
{
	/*
	 * Drain EVERY waiting byte, not just one: a paste delivers several at
	 * once. LSR is re-read on every iteration, so the loop ends exactly when
	 * the UART's FIFO is empty - which is also what lowers its interrupt line.
	 */
	while (*(volatile uint8_t *)UART_LSR & UART_LSR_DR) {
		/* Reading RBR removes the byte from the UART. Always read it, even
		 * when the ring is full - otherwise the line stays raised forever. */
		char c = *(volatile uint8_t *)UART_RBR;

		/* QEMU sends '\r' for Enter; hand the shell a plain '\n'. */
		if (c == '\r') c = '\n';

		/*
		 * Store and echo only if there is room. When the ring is full the
		 * byte is dropped (oldest input survives) and NOT echoed, so the
		 * screen never shows input that the reader won't receive.
		 */
		if (rx_head - rx_tail < UART_RX_SIZE) {
			rx_buf[rx_head % UART_RX_SIZE] = c;
			rx_head++;
			sbi_putchar(c);
		}
	}

	/*
	 * Wake every process sleeping on the ring. Harmless when nobody is
	 * waiting; woken readers re-run their read() and find the new bytes.
	 */
	wakeup(&rx_buf);
}

/*
 * uart_getc() - Take the next byte from the ring, or -1 if it is empty.
 */
int uart_getc(void)
{
	if (rx_head == rx_tail) return -1;

	/*
	 * unsigned char so a byte >= 128 is returned as 128..255 - a plain char
	 * could come out negative and be mistaken for the -1 "empty" result.
	 */
	unsigned char c = rx_buf[rx_tail % UART_RX_SIZE];
	rx_tail++;
	return c;
}
