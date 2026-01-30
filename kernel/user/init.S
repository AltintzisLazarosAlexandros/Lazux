.section .user.text, "ax"
.global user_entry
user_entry:
  li a7, 0        // SYS_PUTCHAR (we'll implement later)
  li a0, 'H'
  ecall
  li a0, 'i'
  ecall
1: j 1b

