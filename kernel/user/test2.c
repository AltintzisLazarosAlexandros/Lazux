#include "lazux.h"

int main() {
    puts("--- Lazux Memory Allocator Test ---\n");
    
    // 1. Κάνουμε allocate
    char* str1 = (char*)malloc(100);
    puts("Allocated str1 at: ");
    putint((int)(uintptr_t)str1);
    puts("\n");

    // 2. Κάνουμε allocate άλλο ένα
    char* str2 = (char*)malloc(100);
    puts("Allocated str2 at: ");
    putint((int)(uintptr_t)str2);
    puts("\n");

    // 3. Ελευθερώνουμε το πρώτο!
    puts("Freeing str1...\n");
    free(str1);

    // 4. Κάνουμε νέο allocate. ΑΝ ΔΟΥΛΕΥΕΙ ΣΩΣΤΑ, θα πρέπει να μας δώσει την ΙΔΙΑ διεύθυνση με το str1!
    char* str3 = (char*)malloc(100);
    puts("Allocated str3 at: ");
    putint((int)(uintptr_t)str3);
    puts("\n");
    
    if (str1 == str3) {
        puts("[SUCCESS] Memory was successfully recycled!\n");
    } else {
        puts("[FAIL] Allocator gave new memory instead of recycling.\n");
    }
    
    free(str2);
    free(str3);

    exit(0);
    return 0;
}