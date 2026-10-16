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
        if(ret == 0) break;
        if (buffer[len - 1] == '\n')      // got the end of the line? done
            break;
    }

    buffer[len] = '\0';                   // end marker right after the text
    return len;                           // how many bytes we stored
}

int parseline(char *line, char **args, int max_args){
    int argc = 0, i = 0;
    while(line[i] != '\0' && max_args > argc){
        while(line[i] == ' ') i++;
        if (line[i] == '\0') break;
        args[argc++] = &line[i];
        while(line[i] != ' ' && line[i] != '\0'){
            i++;
        }
        if (line[i] == ' ') {
                line[i] = '\0';   // end this word here
                i++;              // continue after it
        }
    }
    args[argc] = 0;
    return argc;
}

int main() {
    char line[MAX];
    char *args[17];
    while (1) {
        printf("lazux> ");
        int n = read_line(line, sizeof(line));
        if (n < 0) return -1;
        if (n == 0) {
            printf("\n");
            return 0;
        }
        if (n > 0 && line[n - 1] == '\n')
            line[n - 1] = '\0';           // drop the newline
        int argc = parseline(line, args, 16);
        if(argc == 0){
            continue;
        }
        if(ms_compare(args[0], "exit") == 0 ) {
            if(argc == 1){
                return 0;
            }else{
                lz_error("exit", "too many arguments");
            }
        }else {
            for(int i = 0; i < argc; i++){
                printf("arg[%d] = [%s]\n", i, args[i]);
            }
        }
    }
    return 0;
}
