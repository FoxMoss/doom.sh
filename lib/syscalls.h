#pragma once
#include <stddef.h>
#include <stdio.h>

void sys_write(const char *str, size_t len);

static inline void sys_memcpy(char *dest, const char *src, size_t len) {
  asm volatile("li a7, 68;"
               "mv a1, %[dest];"
               "mv a2, %[src];"
               "mv a3, %[len];"
               "ecall;"
               :
               : [dest] "r"(dest), [src] "r"(src), [len] "r"(len)
               : "a7", "a1", "a2", "a3");
}
void sys_drawpatch(void *column, size_t screenwidth, void *desttop);

int sys_ticks();
char sys_char();
