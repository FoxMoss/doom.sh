#include "syscalls.h"
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALIGN (sizeof(size_t))
#define ONES ((size_t)-1 / UCHAR_MAX)
#define HIGHS (ONES * (UCHAR_MAX / 2 + 1))
#define HASZERO(x) ((x) - ONES & ~(x) & HIGHS)

size_t strlen(const char *s) {
  const char *a = s;
#ifdef __GNUC__
  typedef size_t __attribute__((__may_alias__)) word;
  const word *w;
  for (; (uintptr_t)s % ALIGN; s++)
    if (!*s)
      return s - a;
  for (w = (const void *)s; !HASZERO(*w); w++)
    ;
  s = (const void *)w;
#endif
  for (; *s; s++)
    ;
  return s - a;
}

void *memset(void *dest, int c, size_t n) {
  unsigned char *s = dest;
  size_t k;

  /* Fill head and tail with minimal branching. Each
   * conditional ensures that all the subsequently used
   * offsets are well-defined and in the dest region. */

  if (!n)
    return dest;
  s[0] = c;
  s[n - 1] = c;
  if (n <= 2)
    return dest;
  s[1] = c;
  s[2] = c;
  s[n - 2] = c;
  s[n - 3] = c;
  if (n <= 6)
    return dest;
  s[3] = c;
  s[n - 4] = c;
  if (n <= 8)
    return dest;

  /* Advance pointer to align it at a 4-byte boundary,
   * and truncate n to a multiple of 4. The previous code
   * already took care of any head/tail that get cut off
   * by the alignment. */

  k = -(uintptr_t)s & 3;
  s += k;
  n -= k;
  n &= -4;

#ifdef __GNUC__
  typedef uint32_t __attribute__((__may_alias__)) u32;
  typedef uint64_t __attribute__((__may_alias__)) u64;

  u32 c32 = ((u32)-1) / 255 * (unsigned char)c;

  /* In preparation to copy 32 bytes at a time, aligned on
   * an 8-byte bounary, fill head/tail up to 28 bytes each.
   * As in the initial byte-based head/tail fill, each
   * conditional below ensures that the subsequent offsets
   * are valid (e.g. !(n<=24) implies n>=28). */

  *(u32 *)(s + 0) = c32;
  *(u32 *)(s + n - 4) = c32;
  if (n <= 8)
    return dest;
  *(u32 *)(s + 4) = c32;
  *(u32 *)(s + 8) = c32;
  *(u32 *)(s + n - 12) = c32;
  *(u32 *)(s + n - 8) = c32;
  if (n <= 24)
    return dest;
  *(u32 *)(s + 12) = c32;
  *(u32 *)(s + 16) = c32;
  *(u32 *)(s + 20) = c32;
  *(u32 *)(s + 24) = c32;
  *(u32 *)(s + n - 28) = c32;
  *(u32 *)(s + n - 24) = c32;
  *(u32 *)(s + n - 20) = c32;
  *(u32 *)(s + n - 16) = c32;

  /* Align to a multiple of 8 so we can fill 64 bits at a time,
   * and avoid writing the same bytes twice as much as is
   * practical without introducing additional branching. */

  k = 24 + ((uintptr_t)s & 4);
  s += k;
  n -= k;

  /* If this loop is reached, 28 tail bytes have already been
   * filled, so any remainder when n drops below 32 can be
   * safely ignored. */

  u64 c64 = c32 | ((u64)c32 << 32);
  for (; n >= 32; n -= 32, s += 32) {
    *(u64 *)(s + 0) = c64;
    *(u64 *)(s + 8) = c64;
    *(u64 *)(s + 16) = c64;
    *(u64 *)(s + 24) = c64;
  }
#else
  /* Pure C fallback with no aliasing violations. */
  for (; n; n--, s++)
    *s = c;
#endif

  return dest;
}

void *memcpy(void *restrict dest, const void *restrict src, size_t n) {
  sys_memcpy(dest, src, n);
  return dest;
}

int puts(const char *s) {
  size_t len = strlen(s);
  sys_write(s, len);
  putchar('\n');
  return 0;
}
int putchar(int c) {
  static char *data_hack = "\0\0";
  data_hack[0] = c;
  sys_write(data_hack, 1);
  return 1;
}

int sys_ticks() {
  int ticks;
  asm volatile("li a7, 65;"
               "ecall;"
               "mv %[ticks], a1;"
               : [ticks] "=r"(ticks)
               :
               : "a7", "a1");
  return ticks;
}

char sys_char() {
  char c;
  asm volatile("li a7, 66;"
               "ecall;"
               "mv %[c], a1;"
               : [c] "=r"(c)
               :
               : "a7", "a1");
  return c;
}

void exit(int i) {
  asm volatile("li a7, 93;"
               "li a0, 1;"
               "ecall;"
               :
               :
               : "a7", "a0");
}
void sys_savestate() {
  asm volatile("li a7, 92;"
               "ecall;"
               :
               :
               : "a7");
}

int islower(int c) { return (unsigned)c - 'a' < 26; }
int toupper(int c) {
  if (islower(c))
    return c & 0x5f;
  return c;
}

int isupper(int c) { return (unsigned)c - 'A' < 26; }
int tolower(int c) {
  if (isupper(c))
    return c | 32;
  return c;
}

int strcmp(const char *l, const char *r) {
  for (; *l == *r && *l; l++, r++)
    ;
  return *(unsigned char *)l - *(unsigned char *)r;
}
int strcasecmp(const char *_l, const char *_r) {
  const unsigned char *l = (void *)_l, *r = (void *)_r;
  for (; *l && *r && (*l == *r || tolower(*l) == tolower(*r)); l++, r++)
    ;
  return tolower(*l) - tolower(*r);
}
int strncasecmp(const char *_l, const char *_r, size_t n) {
  const unsigned char *l = (void *)_l, *r = (void *)_r;
  if (!n--)
    return 0;
  for (; *l && *r && n && (*l == *r || tolower(*l) == tolower(*r));
       l++, r++, n--)
    ;
  return tolower(*l) - tolower(*r);
}

#define ALIGN (sizeof(size_t))
#define ONES ((size_t)-1 / UCHAR_MAX)
#define HIGHS (ONES * (UCHAR_MAX / 2 + 1))
#define HASZERO(x) ((x) - ONES & ~(x) & HIGHS)

char *__stpncpy(char *restrict d, const char *restrict s, size_t n) {
#ifdef __GNUC__
  typedef size_t __attribute__((__may_alias__)) word;
  word *wd;
  const word *ws;
  if (((uintptr_t)s & ALIGN) == ((uintptr_t)d & ALIGN)) {
    for (; ((uintptr_t)s & ALIGN) && n && (*d = *s); n--, s++, d++)
      ;
    if (!n || !*s)
      goto tail;
    wd = (void *)d;
    ws = (const void *)s;
    for (; n >= sizeof(size_t) && !HASZERO(*ws);
         n -= sizeof(size_t), ws++, wd++)
      *wd = *ws;
    d = (void *)wd;
    s = (const void *)ws;
  }
#endif
  for (; n && (*d = *s); n--, s++, d++)
    ;
tail:
  memset(d, 0, n);
  return d;
}
char *strncpy(char *restrict d, const char *restrict s, size_t n) {
  __stpncpy(d, s, n);
  return d;
}

char *__stpcpy(char *restrict d, const char *restrict s) {
#ifdef __GNUC__
  typedef size_t __attribute__((__may_alias__)) word;
  word *wd;
  const word *ws;
  if ((uintptr_t)s % ALIGN == (uintptr_t)d % ALIGN) {
    for (; (uintptr_t)s % ALIGN; s++, d++)
      if (!(*d = *s))
        return d;
    wd = (void *)d;
    ws = (const void *)s;
    for (; !HASZERO(*ws); *wd++ = *ws++)
      ;
    d = (void *)wd;
    s = (const void *)ws;
  }
#endif
  for (; (*d = *s); s++, d++)
    ;

  return d;
}

char *strcat(char *restrict dest, const char *restrict src) {
  strcpy(dest + strlen(dest), src);
  return dest;
}
char *strcpy(char *restrict dest, const char *restrict src) {
  __stpcpy(dest, src);
  return dest;
}

static int parse_number(char c) {
  if ('0' <= c && c <= '9')
    return c & 0x0F; // first nibble is a number
  return -1;
}

static bool will_overflow_add(int a, int b) {
  return a >= 0 ? b > INT_MAX - a : b < INT_MIN - a;
}
static bool will_overflow_mult(int a, int b) {
  int x = a * b;
  return a != 0 && x / a != b;
}

// custom atoi impl because why not
int atoi(const char *s) {
  size_t s_len = strlen(s);
  enum { STATE_SIGN, STATE_NUMBER } state = STATE_SIGN;
  int8_t sign = 1;
  const char *start_ptr = s;
  const char *end_ptr = s;
  for (size_t c = 0; c < s_len; c++) {
    if (state == STATE_SIGN) {
      if (' ' == s[c])
        continue;

      start_ptr = s + c;
      if ('-' == s[c]) {
        sign = -1;
        state = STATE_NUMBER;
        continue;
      } else if ('+' == s[c]) {
        state = STATE_NUMBER;
        continue;
      }

      if (parse_number(s[c]) != -1) {
        state = STATE_NUMBER;
        start_ptr = s + c - 1;
      } else {
        return 0; // starts with a non sign
      }
    }
    if (state == STATE_NUMBER) {
      int parsed_num = parse_number(s[c]);
      if (parsed_num == -1) {
        break;
      }
      end_ptr = s + c;
    }
  }

  uint32_t place = 1;
  int accumulator = 0;
  int buffer_accumulator = 0;

  bool overflow = false;
  for (; end_ptr > start_ptr && end_ptr >= s; end_ptr--) {
    int parsed = parse_number(*end_ptr);
    if (parsed == 0 && overflow == true) {
      continue;
    }
    if (__builtin_mul_overflow(parsed, place, &parsed)) {
      overflow = true;
    }

    if (!will_overflow_add(parsed, accumulator)) {
      buffer_accumulator += parsed;
    } else {
      overflow = true;
    }

    if (accumulator > buffer_accumulator) {
      overflow = true;
    }

    if (overflow) {
      if (sign < 0) {
        return INT_MIN;
      }
      return INT_MAX;
    }
    overflow = false;
    if (__builtin_mul_overflow(place, 10, &place)) {
      overflow = true;
    }
    accumulator = buffer_accumulator;
  }
  return accumulator * sign;
}
int strncmp(const char *_l, const char *_r, size_t n) {
  const unsigned char *l = (void *)_l, *r = (void *)_r;
  if (!n--)
    return 0;
  for (; *l && *r && n && *l == *r; l++, r++, n--)
    ;
  return *l - *r;
}

int fprintf(FILE *__restrict __stream, const char *__restrict __format, ...) {
  return 0; // no filesystem
}
size_t fwrite(const void *__restrict ptr, size_t size, size_t n,
              FILE *__restrict s) {
  return 0;
}
int fputs(const char *restrict s, FILE *restrict stream) { return 0; }
int fputc(int c, FILE *stream) { return 0; }
int fflush(FILE *stream) { return 0; }
FILE *fopen(const char *__restrict filename, const char *__restrict modes) {
  return NULL;
}
int fclose(FILE *stream) {}

void sys_drawpatch(void *column, size_t screenwidth, void *desttop) {
  asm volatile("li a7, 69;"
               "mv a1, %[column];"
               "mv a2, %[screenwidth];"
               "mv a3, %[desttop];"
               "ecall;"
               :
               : [column] "r"(column), [screenwidth] "r"(screenwidth),
                 [desttop] "r"(desttop)
               : "a7", "a1", "a2", "a3");
}
// lump_p=${REGS[11]}
// lump_info=${REGS[12]}
// lump_length=${REGS[13]}
// name=${REGS[14]}
size_t sys_findlump(void *lump_p, void *lump_info, size_t lump_length,
                    void *name, size_t numlumps) {
  size_t lump_index;
  asm volatile("li a7, 70;"
               "mv a1, %[lump_p];"
               "mv a2, %[lump_info];"
               "mv a3, %[lump_length];"
               "mv a4, %[name];"
               "mv a5, %[numlumps];"
               "ecall;"
               "mv %[lump_index], a1;"
               : [lump_index] "=r"(lump_index)
               : [lump_p] "r"(lump_p), [lump_info] "r"(lump_info),
                 [lump_length] "r"(lump_length), [name] "r"(name),
                 [numlumps] "r"(numlumps)
               : "a7", "a1", "a2", "a3", "a4", "a5");
  return lump_index;
}

#include "_divsi3.c"
#include "printf.c"
#include "tinalloc.c"
