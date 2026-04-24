.section .text
.globl trap_entry
.align 2
trap_entry:
    # Swap t0 <-> sscratch:
    #   t0 = trapframe pointer (must be pre-initialized in kmain)
    #   sscratch = original interrupted t0
    csrrw t0, sscratch, t0

    # Save GPRs (use t0 as tf base)
    sd ra, 0(t0)
    sd sp, 8(t0)
    sd gp, 16(t0)
    sd tp, 24(t0)

    # Save t1/t2 BEFORE clobbering t1 for scratch/CSR reads
    sd t1, 40(t0)
    sd t2, 48(t0)

    # Save original t0 from sscratch into tf->t0
    csrr t1, sscratch
    sd   t1, 32(t0)

    sd s0, 56(t0)
    sd s1, 64(t0)

    sd a0, 72(t0)
    sd a1, 80(t0)
    sd a2, 88(t0)
    sd a3, 96(t0)
    sd a4, 104(t0)
    sd a5, 112(t0)
    sd a6, 120(t0)
    sd a7, 128(t0)

    sd s2, 136(t0)
    sd s3, 144(t0)
    sd s4, 152(t0)
    sd s5, 160(t0)
    sd s6, 168(t0)
    sd s7, 176(t0)
    sd s8, 184(t0)
    sd s9, 192(t0)
    sd s10, 200(t0)
    sd s11, 208(t0)

    sd t3, 216(t0)
    sd t4, 224(t0)
    sd t5, 232(t0)
    sd t6, 240(t0)

    # Save CSRs
    csrr t1, sepc
    sd   t1, 248(t0)
    csrr t1, sstatus
    sd   t1, 256(t0)
    csrr t1, scause
    sd   t1, 264(t0)
    csrr t1, stval
    sd   t1, 272(t0)

    # Call C handler(tf)
    mv a0, t0

    ld sp, 280(t0)

    call trap_handler

    mv t0, a0

    # Restore CSRs (handler may have changed sepc/sstatus)
    ld t1, 248(t0)
    csrw sepc, t1
    ld t1, 256(t0)
    csrw sstatus, t1

    # Restore GPRs (restore sp late; restore t0 last)
    ld ra, 0(t0)
    ld gp, 16(t0)
    ld tp, 24(t0)

    ld t1, 40(t0)
    ld t2, 48(t0)

    ld s0, 56(t0)
    ld s1, 64(t0)

    ld a0, 72(t0)
    ld a1, 80(t0)
    ld a2, 88(t0)
    ld a3, 96(t0)
    ld a4, 104(t0)
    ld a5, 112(t0)
    ld a6, 120(t0)
    ld a7, 128(t0)

    ld s2, 136(t0)
    ld s3, 144(t0)
    ld s4, 152(t0)
    ld s5, 160(t0)
    ld s6, 168(t0)
    ld s7, 176(t0)
    ld s8, 184(t0)
    ld s9, 192(t0)
    ld s10, 200(t0)
    ld s11, 208(t0)

    ld t3, 216(t0)
    ld t4, 224(t0)
    ld t5, 232(t0)
    ld t6, 240(t0)

    # Restore sp
    ld sp, 8(t0)

    # IMPORTANT: set sscratch back to tf pointer for next trap
    csrw sscratch, t0

    # Restore original t0 last
    ld t0, 32(t0)

    sret

