/*
 * uart.c - NS16550A UART driver (console input with line editing)
 *
 * Producer/consumer split around a receive ring buffer:
 *
 *   uart_intr()  - PRODUCER, interrupt time. Runs the moment bytes arrive,
 *                  moves them out of the UART's tiny FIFO into the ring, echoes
 *                  them, applies line editing, and wakes readers when Enter
 *                  completes a line.
 *   uart_getc()  - CONSUMER, syscall time (console SYS_READ). Takes bytes out
 *                  of finished lines whenever a process asks. Never blocks.
 *
 * The ring decouples the two: keys typed while no one is reading wait in the
 * ring, and a reader that finds no finished line sleeps until uart_intr()
 * commits one.
 *
 * Canonical ("cooked") mode, like a Unix tty's default: input is handed to
 * readers a whole line at a time, and the line being typed can be edited with
 * Backspace before Enter commits it. Escape sequences (arrow keys, Home, Delete,
 * ...) and other control characters are swallowed. Cursor movement, history and
 * other rich editing belong in user space (a shell's line editor), as in Linux,
 * where bash/readline do them - not the kernel.
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
 * Three free-running counters - rx_tail and rx_commit only ever go UP; rx_head
 * goes up on a typed byte and back down on Backspace, but never below rx_commit:
 *
 *   rx_tail   = total bytes ever taken by readers   (changed only by uart_getc)
 *   rx_commit = end of the finished (committed) lines (changed only by uart_intr)
 *   rx_head   = end of everything typed so far       (changed only by uart_intr)
 *
 *          rx_tail          rx_commit           rx_head
 *             |  committed lines |  line being typed  |
 *             |   (readable)     |    (editable)      |
 *
 * Invariants: rx_tail <= rx_commit <= rx_head, and rx_head - rx_tail <= UART_RX_SIZE.
 *
 *   readable bytes = rx_commit - rx_tail   (uart_getc: empty when they are equal)
 *   editable bytes = rx_head - rx_commit   (Backspace can erase only these)
 *   ring use       = rx_head - rx_tail
 *   slot of n      = n % UART_RX_SIZE
 *
 * Backspace stops at rx_commit: a committed line may already be partly in a
 * reader's buffer, so it can never be taken back.
 *
 * UART_RX_SIZE must be a power of two so that '% UART_RX_SIZE' stays correct
 * when the uint32_t counters eventually wrap around. All variables live in
 * .bss, which entry.S zeroes, so the ring starts empty with no setup.
 *
 * &rx_buf doubles as the wait channel: readers sleep_on(uart_rx_chan()),
 * which is &rx_buf, and uart_intr() calls wakeup(&rx_buf).
 */
#define UART_RX_SIZE 128

static char rx_buf[UART_RX_SIZE];
static uint32_t rx_head;
static uint32_t rx_tail;
static uint32_t rx_commit;

/*
 * Escape-sequence state. Special keys send multi-byte sequences beginning with
 * ESC (0x1b), e.g. Up = ESC '[' 'A', Delete = ESC '[' '3' '~'. A lone '[' or
 * 'A' can't be told apart from typed text, so the driver remembers where it is
 * inside a sequence and drops every byte of it:
 *
 *   ESC_NORMAL  - not in a sequence. ESC -> ESC_GOT_ESC.
 *   ESC_GOT_ESC - previous byte was ESC. '[' -> ESC_IN_SEQ, anything else ->
 *                 ESC_NORMAL (a two-byte sequence like Alt+key).
 *   ESC_IN_SEQ  - inside "ESC [". Parameter bytes ('0'-'9', ';', ...) keep us
 *                 here; a final byte (0x40-0x7e: letters, '~') ends it.
 *
 * Static, not a loop-local: the bytes of one key usually arrive in a single
 * interrupt, but nothing guarantees it - ESC may come at the end of one
 * uart_intr() call and "[A" in the next.
 */
#define ESC_NORMAL  0
#define ESC_GOT_ESC 1
#define ESC_IN_SEQ  2

#define KEY_ESC 0x1b
#define KEY_DEL 0x7f   /* what QEMU/most terminals send for Backspace */

static int esc_state = ESC_NORMAL;

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
		 * when the ring is full - otherwise the line stays raised forever.
		 * unsigned char: bytes >= 0x80 (UTF-8) must compare as 128..255, not
		 * as negative values that would look like control characters. */
		unsigned char c = *(volatile uint8_t *)UART_RBR;

		/* QEMU sends '\r' for Enter; hand the shell a plain '\n'. */
		if (c == '\r') c = '\n';

		/*
		 * 1. Escape sequences: checked first, because while a sequence is in
		 *    progress a byte belongs to it whatever its value. Every byte of
		 *    a sequence is dropped - not stored, not echoed.
		 */
		if (esc_state == ESC_GOT_ESC) {
			esc_state = (c == '[') ? ESC_IN_SEQ : ESC_NORMAL;
			continue;
		}
		if (esc_state == ESC_IN_SEQ) {
			if (c >= 0x40 && c <= 0x7e) esc_state = ESC_NORMAL;
			continue;
		}
		if (c == KEY_ESC) {
			esc_state = ESC_GOT_ESC;
			continue;
		}

		if (c == '\n') {
			/*
			 * 2. Enter: store the '\n' and commit the whole line.
			 *
			 * '\n' may use the LAST free slot, which normal bytes never take
			 * (see 5.) - so while a line is being typed there is always room
			 * for the Enter that ends it. Without that reserve a full ring
			 * holding an unfinished line would deadlock: nothing committed,
			 * so the reader sleeps, and no room left for the '\n'.
			 *
			 * The commit comes AFTER the '\n' is stored, so every line a
			 * reader receives ends with its newline.
			 */
			if (rx_head - rx_tail < UART_RX_SIZE) {
				rx_buf[rx_head % UART_RX_SIZE] = c;
				rx_head++;
				rx_commit = rx_head;
				sbi_putchar(c);
				/*
				 * Wake every process sleeping on the ring - only here, on a
				 * commit: before Enter there is nothing a reader may take.
				 */
				wakeup(&rx_buf);
			}
		} else if (c == '\b' || c == KEY_DEL) {
			/*
			 * 3. Backspace: erase the last byte of the line being typed.
			 * Never past rx_commit (committed lines may already be read), and
			 * no echo when there is nothing to erase, so the cursor can't walk
			 * back into a prompt. "\b \b" = cursor left, blank the character,
			 * cursor left again. Frees a slot, so no room check.
			 */
			if (rx_head != rx_commit) {
				rx_head--;
				sbi_putchar('\b');
				sbi_putchar(' ');
				sbi_putchar('\b');
			}
		} else if (c < 0x20) {
			/*
			 * 4. Any other control character (Tab, Ctrl-C, ...): dropped, not
			 * echoed. '\n' was handled above.
			 */
		} else if (rx_head - rx_tail < UART_RX_SIZE - 1) {
			/*
			 * 5. Normal byte: store and echo if there is room, keeping the last
			 * slot free for '\n' (see 2.). Otherwise drop it WITHOUT echo, so
			 * the screen never shows input the reader won't receive.
			 */
			rx_buf[rx_head % UART_RX_SIZE] = c;
			rx_head++;
			sbi_putchar(c);
		}
	}
}

/*
 * uart_getc() - Take the next byte of a committed line, or -1 if there is none.
 *
 * Bytes between rx_commit and rx_head (the line still being typed) are not
 * visible here: a reader only ever sees finished lines.
 */
int uart_getc(void)
{
	if (rx_commit == rx_tail) return -1;

	/*
	 * unsigned char so a byte >= 128 is returned as 128..255 - a plain char
	 * could come out negative and be mistaken for the -1 "empty" result.
	 */
	unsigned char c = rx_buf[rx_tail % UART_RX_SIZE];
	rx_tail++;
	return c;
}

/*
 * uart_rx_chan() - The wait channel for "a line was committed to the RX ring".
 *
 * rx_buf stays static; callers outside the driver get only its address, as an
 * opaque void * to pass to sleep_on(). It must be the same pointer uart_intr()
 * hands to wakeup(), which is why the driver - not the caller - names it.
 */
void *uart_rx_chan(void)
{
	return &rx_buf;
}
