.section .text
.globl switch_to_user

switch_to_user:
    csrw sscratch, a0

    ld t0, 248(a0)
    csrw sepc, t0
    csrr t0, sstatus
    li t1, ~0x100      
    and t0, t0, t1
    li t1, 0x20        
    or t0, t0, t1
    csrw sstatus, t0
    csrw satp, a1
    sfence.vma

    ld ra, 0(a0)      # x1
    ld sp, 8(a0)      # x2
    ld gp, 16(a0)     # x3
    ld tp, 24(a0)     # x4
    ld t0, 32(a0)     # x5
    ld t1, 40(a0)     # x6
    ld t2, 48(a0)     # x7
    ld s0, 56(a0)     # x8
    ld s1, 64(a0)     # x9
    ld a1, 80(a0)     # x11
    ld a2, 88(a0)     # x12
    ld a3, 96(a0)     # x13
    ld a4, 104(a0)    # x14
    ld a5, 112(a0)    # x15
    ld a6, 120(a0)    # x16
    ld a7, 128(a0)    # x17
    ld s2, 136(a0)    # x18
    ld s3, 144(a0)    # x19
    ld s4, 152(a0)    # x20
    ld s5, 160(a0)    # x21
    ld s6, 168(a0)    # x22
    ld s7, 176(a0)    # x23
    ld s8, 184(a0)    # x24
    ld s9, 192(a0)    # x25
    ld s10, 200(a0)   # x26
    ld s11, 208(a0)   # x27
    ld t3, 216(a0)    # x28
    ld t4, 224(a0)    # x29
    ld t5, 232(a0)    # x30
    ld t6, 240(a0)    # x31
    ld a0, 72(a0)     # x10

    sret
