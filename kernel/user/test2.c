#include "lazux.h"

int main() {
    int pid = fork();
    if (pid == 0) {
        puts("test2: I am the Child! Exiting now.\n");
        exit(0);
    } else {
        puts("test2: I am the Parent! Waiting for my child...\n");
        wait();
        puts("test2: Child is dead. I am exiting too.\n");
        exit(0);
    }
    return 0; 
}