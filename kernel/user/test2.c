#include "lazux.h"

int main() {
    puts("--- Lazux VFS Test ---\n");
    
    int fd = open("init.elf");
    if (fd < 0) {
        puts("Error: Could not open file!\n");
        exit(1);
    }

    puts("Successfully opened init.elf! Got FD: ");
    putint(fd);
    puts("\n");

    // Θα διαβάσουμε τα πρώτα 4 bytes (το ELF Magic Header)
    char buffer[5];
    int bytes_read = read(fd, buffer, 4);
    
    // Προσθέτουμε το null-terminator για να το τυπώσουμε ως string
    buffer[4] = '\0'; 

    puts("Bytes read: ");
    putint(bytes_read);
    puts("\nData: ");
    
    // Επειδή το πρώτο byte (0x7F) δεν είναι εκτυπώσιμος χαρακτήρας, 
    // τυπώνουμε τα επόμενα 3 (το "ELF")
    puts(&buffer[1]); 
    puts("\n");

    exit(0);
    return 0;
}