/*
 * user/readtest.c - Console input test (Stage B step 5)
 *
 * Console SYS_READ is "raw": it returns as soon as at least one byte is in the
 * RX ring, and stops after a '\n'. A human types far slower than the CPU, so
 * interactively almost every read() returns a single keystroke; only input that
 * is already queued (a paste) comes back in bigger pieces. Lines are therefore
 * assembled HERE, in user space, from however many reads it takes.
 *
 * Checks by hand:
 *   - blocks quietly while nothing is typed (no busy loop)
 *   - every typed byte arrives exactly once, in order, nothing lost
 *   - a paste of several lines comes back one line per read() (see 'v')
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

/* Print n bytes between brackets, with '\n' made visible. */
static void show(const char *p, int n) {
    putchar('[');
    for (int i = 0; i < n; i++) {
        if (p[i] == '\n') printf("\\n");
        else putchar(p[i]);
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
