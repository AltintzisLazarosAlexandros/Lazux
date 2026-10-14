/*
 * user/mystring.c - String functions for user programs
 *
 * Ported from the CS-255 assignment 2 string module (mystring_ptrs.c, same
 * author). Declarations are in lazux.h; every user program links this file.
 *
 * Changes from the original:
 * - No assert(): user programs are built -nostdlib, so there is no
 *   __assert_func to link against. A NULL argument now dereferences address 0,
 *   which page-faults in U-mode and the kernel kills the program - still a
 *   "checked runtime error", just reported by the kernel instead.
 * - ms_compare/ms_ncompare compare as unsigned char, so bytes >= 0x80 (UTF-8)
 *   sort after ASCII instead of coming out negative.
 * - ms_concat: dropped a counter that was incremented but never read.
 *
 * The ms_ prefix keeps these apart from the compiler's builtin strcmp/strlen
 * and from the private strlen() in syscalls.c.
 */

#include "lazux.h"

/* Return the length of string `pcStr`. */
size_t ms_length(const char *pcStr){
   const char *pcStrEnd = pcStr;
   while (*pcStrEnd != '\0')
      pcStrEnd++;
   return pcStrEnd - pcStr;
}

/* Copies the string from 'pcSrc' to 'pcDest'. */
void ms_copy(char *pcDest, const char *pcSrc){
   while (*pcSrc != '\0')
   {
      *pcDest = *pcSrc;
      pcDest++;
      pcSrc++;
   }
   *pcDest = '\0';
}

/* Copies at most n characters from 'pcSrc' to 'pcDest', padding with '\0'
   up to n (like strncpy: no '\0' is added if pcSrc is n or more long). */
void ms_ncopy(char *pcDest, const char *pcSrc, size_t n){
   size_t len = 0U;
   while (*pcSrc != '\0' && len < n)
   {
      *pcDest = *pcSrc;
      pcDest++;
      pcSrc++;
      len++;
   }
   while (len < n)
   {
      *pcDest = '\0';
      pcDest++;
      len++;
   }
}

/* Appends the string from 'pcSrc' to the end of 'pcDest'. */
void ms_concat(char *pcDest, const char *pcSrc){
   while (*pcDest != '\0')
      pcDest++;
   ms_copy(pcDest, pcSrc);
}

/* Appends 'pcSrc' to 'pcDest' until the total length of 'pcDest' reaches n,
   then terminates it. */
void ms_nconcat(char *pcDest, const char *pcSrc, size_t n){
   size_t len = 0U;
   while (*pcDest != '\0')
   {
      len++;
      pcDest++;
   }
   while (*pcSrc != '\0' && len < n)
   {
      *pcDest = *pcSrc;
      pcDest++;
      pcSrc++;
      len++;
   }
   *pcDest = '\0';
}

/* Compares 'pcStr1' and 'pcStr2': 0 if equal, < 0 if 'pcStr1' sorts first,
   > 0 if 'pcStr2' sorts first. */
int ms_compare(const char *pcStr1, const char *pcStr2){
   while (*pcStr1 != '\0' && *pcStr2 != '\0')
   {
      if (*pcStr1 != *pcStr2)
      {
         return (unsigned char)*pcStr1 - (unsigned char)*pcStr2;
      }
      pcStr1++;
      pcStr2++;
   }
   return (unsigned char)*pcStr1 - (unsigned char)*pcStr2;
}

/* Like ms_compare, but compares at most the first n characters. */
int ms_ncompare(const char *pcStr1, const char *pcStr2, size_t n){
   size_t i = 0;
   while (i < n && *pcStr1 != '\0' && *pcStr2 != '\0')
   {
      if (*pcStr1 != *pcStr2)
      {
         return (unsigned char)*pcStr1 - (unsigned char)*pcStr2;
      }
      pcStr1++;
      pcStr2++;
      i++;
   }
   if (i == n)
   {
      return 0;
   }
   return (unsigned char)*pcStr1 - (unsigned char)*pcStr2;
}

/* Finds the first occurrence of 'pcNeedle' in 'pcHaystack', returning a
   pointer to it, or 0 (NULL) if not found. */
const char *ms_search(const char *pcHaystack, const char *pcNeedle){
   while (*pcHaystack != '\0')
   {
      const char *p = pcHaystack;
      const char *q = pcNeedle;
      while (*p == *q && *q != '\0')
      {
         p++;
         q++;
      }
      if (*q == '\0')
      {
         return pcHaystack;
      }
      pcHaystack++;
   }
   return 0;
}
