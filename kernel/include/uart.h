/*
 * uart.h - NS16550A UART driver interface (console input)
 *
 * QEMU virt's UART0 is a 16550-compatible serial port - the same one OpenSBI
 * prints through. Lazux uses it for console INPUT: each received byte raises
 * IRQ 10 at the PLIC, plic_dispatch() routes it to uart_intr(), which stores
 * the byte in a receive ring buffer and wakes any process waiting to read.
 *
 * Console OUTPUT (including echo of typed characters) still goes through
 * OpenSBI (sbi_putchar), so there is a single writer to the device.
 *
 * Every 16550 register is 8 bits wide: access them as 'volatile uint8_t *',
 * never 32-bit like the PLIC.
 */

#pragma once
#include "platform.h"

/*
 * Register addresses: UART0_BASE + fixed offset (no index, unlike the PLIC).
 *
 * RBR and THR share offset 0 - the 16550 tells them apart by direction:
 * a READ at +0 returns the next received byte (RBR), a WRITE at +0 sends a byte
 * (THR). Both names exist so the code says which one it means.
 */
#define UART_RBR (UART0_BASE + 0)   /* Receive Buffer Register  (read)  */
#define UART_THR (UART0_BASE + 0)   /* Transmit Holding Register (write) - unused while output goes via OpenSBI */
#define UART_IER (UART0_BASE + 1)   /* Interrupt Enable Register */
#define UART_LSR (UART0_BASE + 5)   /* Line Status Register      */

/*
 * Bit masks, prefixed with the register they belong to. UART_IER_RX and
 * UART_LSR_DR are both 0x01 but are different bits in different registers.
 */
#define UART_IER_RX   0x01  /* IER bit 0: interrupt when received data is available */
#define UART_LSR_DR   0x01  /* LSR bit 0: Data Ready - at least one byte waits in RBR */
#define UART_LSR_THRE 0x20  /* LSR bit 5: THR Empty - safe to write the next byte to THR */

/*
 * uart_init() - Enable the UART's receive interrupt.
 *
 * Sets IER bit 0 so the UART raises its line (IRQ 10) whenever a byte arrives.
 * Call after plic_init(); nothing reaches the hart until SEIE is set in sie.
 */
void uart_init(void);

/*
 * uart_intr() - UART interrupt handler.
 *
 * Called from plic_dispatch() for UART0_IRQ, BEFORE the PLIC complete. Drains
 * every waiting byte (loop while LSR has Data Ready) into the RX ring, echoes
 * it, and wakes processes sleeping on the ring. Draining is what lowers the
 * UART's interrupt line; skip it and the IRQ re-fires forever.
 */
void uart_intr(void);

/*
 * uart_getc() - Take the next byte from the RX ring.
 *
 * Returns the byte (0-255), or -1 if the ring is empty. Never blocks: the
 * caller (SYS_READ on fd 0, step 5) decides whether to sleep_on() the ring
 * and retry.
 */
int uart_getc(void);
