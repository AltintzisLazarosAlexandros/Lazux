#pragma once
#include <stdint.h>

typedef struct trap_frame_t {
  uintptr_t ra;      // 0
  uintptr_t sp;      // 8
  uintptr_t gp;      // 16
  uintptr_t tp;      // 24

  uintptr_t t0;      // 32
  uintptr_t t1;      // 40
  uintptr_t t2;      // 48

  uintptr_t s0;      // 56
  uintptr_t s1;      // 64

  uintptr_t a0;      // 72
  uintptr_t a1;      // 80
  uintptr_t a2;      // 88
  uintptr_t a3;      // 96
  uintptr_t a4;      // 104
  uintptr_t a5;      // 112
  uintptr_t a6;      // 120
  uintptr_t a7;      // 128

  uintptr_t s2;      // 136
  uintptr_t s3;      // 144
  uintptr_t s4;      // 152
  uintptr_t s5;      // 160
  uintptr_t s6;      // 168
  uintptr_t s7;      // 176
  uintptr_t s8;      // 184
  uintptr_t s9;      // 192
  uintptr_t s10;     // 200
  uintptr_t s11;     // 208

  uintptr_t t3;      // 216
  uintptr_t t4;      // 224
  uintptr_t t5;      // 232
  uintptr_t t6;      // 240

  uintptr_t sepc;    // 248
  uintptr_t sstatus; // 256
  uintptr_t scause;  // 264
  uintptr_t stval;   // 272
} trap_frame_t;

void trap_handler(trap_frame_t *tf);
extern trap_frame_t g_tf;

