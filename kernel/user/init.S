.section .user.text, "ax"
.global user_entry
user_entry:
  li a7, 1       
  li a0, 'H'
  ecall
  li a0, 'i'
  ecall
  li a7, 2
  li a0, 0
  ecall
