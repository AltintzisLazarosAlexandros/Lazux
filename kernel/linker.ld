OUTPUT_ARCH(riscv)
ENTRY(_start)

SECTIONS
{
  . = 0x80200000; /* common QEMU virt kernel load address */

  .text : {
    *(.text .text.*)
  }

  .rodata : {
    *(.rodata .rodata.*)
  }

  .data : {
    *(.data .data.*)
  }

 .bss : {
   __bss_start = .;
   *(.bss .bss.*)
   *(COMMON)
   __bss_end = .;
 }

  /* Embedded user payload */
  .user : ALIGN(4096) {
    __user_start = .;
    *(.user.text .user.text.*)
    *(.user.rodata .user.rodata.*)
    __user_end = .;
  }

  /* Single-core kernel stack */
  . = ALIGN(16);
  __stack_bottom = .;
  . += 64K;
  __stack_top = .;

  . = ALIGN(4096);
  _end = .;
}
