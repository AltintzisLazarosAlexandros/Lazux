#include "lazux.h"

int main() {
    printf("Testing SBRK...\n");
    
    char* heap_start = (char*)sbrk(0);
    printf("Initial heap break: %p\n", heap_start);

    char* my_string = (char*)sbrk(20);

    my_string[0] = 'H';
    my_string[1] = 'e';
    my_string[2] = 'l';
    my_string[3] = 'l';
    my_string[4] = 'o';
    my_string[5] = '\n';
    my_string[6] = '\0';
    
    printf("Data from dynamic memory: ");
    printf("%s", my_string);
    
    exit(0);
    return 0;
}