/*
 * user/sh.c - The Lazux shell
 *
 * Step 1: show a prompt, read one line from the console (fd 0), print it back.
 * Later steps add parsing, built-in commands and fork + exec + wait.
 *
 * The console is line-buffered (uart.c): read() blocks until Enter and then
 * returns at most one line, including its '\n'. read_line() still loops,
 * because a line longer than the space left in the buffer comes back in
 * several pieces.
 */
#include "lazux.h"

#define MAX 256

int read_line(char *buffer, int max) {
    int len = 0;                          // nothing stored yet

    while (len < max - 1) {               // room left (one box saved for '\0')
        int ret = read(0, buffer + len, max - 1 - len);
        if (ret < 0) return ret;          // kernel error (any negative number)
        len += ret;                       // we now have 'ret' more bytes
        if (buffer[len - 1] == '\n')      // got the end of the line? done
            break;
    }

    buffer[len] = '\0';                   // end marker right after the text
    return len;                           // how many bytes we stored
}

int main() {
    char line[MAX];
    while (1) {
        printf("lazux> ");
        int n = read_line(line, sizeof(line));
        if (n < 0) return -1;
        if (n > 0 && line[n - 1] == '\n')
            line[n - 1] = '\0';           // drop the newline
        printf("you typed: [%s]\n", line);
    }
    return 0;
}
