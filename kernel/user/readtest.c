/*
 * user/readtest.c - Console input test (Stage B step 5)
 *
 * The console is line-buffered (canonical mode, uart.c): read() blocks until
 * Enter commits a line, then returns at most that one line. With a small read
 * size a line comes back in pieces, so lines are still assembled HERE, in user
 * space, from however many reads it takes.
 *
 * Checks by hand:
 *   - blocks quietly while a line is being typed (no busy loop)
 *   - Backspace edits the line before Enter; it can't erase past the line start
 *   - arrow keys / control characters don't end up in the line
 *   - 4-byte reads split a line, nothing lost, '\n' only in the last piece
 *
 * Commands (typed as a whole line, then Enter):
 *   v  - toggle verbose: also report every raw read() result
 *   s  - toggle small mode: read() asks for only 4 bytes at a time
 *   q  - exit (parent's wait() returns, system halts)
 *
 * The driver echoes keys as they are typed, so the "line" report appears after
 * the echoed text. '\n' is printed as "\n" so read boundaries are visible.
 */
#include "lazux.h"

#define BUF_SIZE 64
#define SMALL_SIZE 4
#define LINE_MAX 128

/*
 * Print n bytes between brackets with every non-printable byte made visible:
 * '\n' as "\n", anything else outside 0x20..0x7e as "\xNN". Printing them raw
 * would let the terminal act on them (ESC sequences move the cursor, DEL/BS
 * erase), hiding exactly the bytes this test exists to catch.
 */
static void show(const char *p, int n) {
    putchar('[');
    for (int i = 0; i < n; i++) {
        unsigned char b = (unsigned char)p[i];
        if (b == '\n') printf("\\n");
        else if (b < 0x20 || b > 0x7e) printf("\\x%x", b);
        else putchar(b);
    }
    putchar(']');
}

int main() {
    char buf[BUF_SIZE];
    char line[LINE_MAX];
    int len = 0;        /* bytes collected in 'line' so far */
    int size = BUF_SIZE;
    int verbose = 0;

    printf("readtest: type lines ('v' = verbose, 's' = 4-byte reads, 'q' = quit)\n");

    for (;;) {
        int n = read(0, buf, size);
        if (n < 0) {
            printf("readtest: read error %d\n", n);
            exit(1);
        }

        if (verbose) {
            printf("  got %d: ", n);
            show(buf, n);
            putchar('\n');
        }

        /* Append to the current line; act on it once a '\n' completes it. */
        for (int i = 0; i < n; i++) {
            if (len < LINE_MAX) line[len++] = buf[i];
            if (buf[i] != '\n') continue;

            printf("line %d: ", len);
            show(line, len);
            putchar('\n');

            if (len == 2 && line[0] == 'q') {
                printf("readtest: bye\n");
                exit(0);
            }
            if (len == 2 && line[0] == 'v') {
                verbose = !verbose;
                printf("readtest: verbose %s\n", verbose ? "on" : "off");
            }
            if (len == 2 && line[0] == 's') {
                size = (size == BUF_SIZE) ? SMALL_SIZE : BUF_SIZE;
                printf("readtest: read size now %d\n", size);
            }
            len = 0;
        }
    }
    return 0;
}
