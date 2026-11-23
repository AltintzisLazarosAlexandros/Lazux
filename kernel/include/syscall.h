#pragma once

// Process Management
#define SYS_EXIT    2
#define SYS_FORK    3
#define SYS_EXEC    4
#define SYS_WAIT    5

// Memory Management
#define SYS_SBRK    6

// File System / I/O
// 1 was SYS_PUTCHAR (console output with no fd at all). Removed in Phase 7b; the number is not reused.
#define SYS_OPEN    7
#define SYS_READ    8
#define SYS_WRITE   9
#define SYS_CLOSE   10