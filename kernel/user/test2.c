#include "lazux.h"

int main() {
    puts("--- Lazux VFS Complete Test ---\n");
    
    int fd = open("init.elf");
    if (fd < 0) {
        puts("Error: Could not open file!\n");
        exit(1);
    }
    char buffer[5];
    int bytes_read = read(fd, buffer, 4);
    printf("%d \n",bytes_read);
    buffer[4] = '\0'; 

    puts("Data read: ");
    puts(&buffer[1]); 
    puts("\n");

    int close_status = close(fd);
    if (close_status == 0) {
        puts("Successfully closed the file descriptor!\n");
    }

    exit(0);
    return 0;
}
