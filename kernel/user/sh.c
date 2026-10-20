/*
 * user/sh.c - The Lazux shell
 *
 * Shows a prompt, reads one line from the console (fd 0) and splits it into
 * words. `exit` is a built-in; any other word names a program in the ramdisk:
 * the shell appends ".elf", forks, the child execs it and the parent waits.
 * If exec fails the child prints why and exits, so only one shell stays alive.
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
    int pid;
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
        if(lz_compare(args[0], "exit") == 0 ) {
            if(argc == 1){
                return 0;
            }else{
                lz_error("exit", "too many arguments");
            }
        }else {
            pid = fork();
            if(pid < 0) lz_error("fork", lz_strerror(pid));
            else if(pid == 0){
                char name[64];
                if(lz_length(args[0]) > 64-5){
                    lz_error(args[0], "Too long of a name");
                    exit(1);
                }
                lz_copy(name, args[0]);
                lz_concat(name, ".elf");
                int res = exec(name);
                lz_error(args[0], lz_strerror(res));
                exit(1);
            }else{
                wait();
                
            }
        }
    }
    return 0;
}
