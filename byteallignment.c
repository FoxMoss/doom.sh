#include <stdint.h>
#include <stdio.h>
struct A {
  uint32_t a; // 4 bytes
  uint32_t b; // 4 bytes
} __attribute__((packed));

struct B {
  char a;     // 1 byte
  uint32_t b; // 4 bytes
} __attribute__((packed));
void _start() {
  struct A a = {};
  struct B b = {};
  printf("sizeof(A)=%lu\n", sizeof(struct A));           // sizeof(A)=8
  printf("sizeof(B)=%lu\n", sizeof(struct B));           // sizeof(B)=8
  printf("A: &b-&a=%ld\n", (void *)&a.b - (void *)&a.a); // A: &b-&a=4
  printf("B: &b-&a=%ld\n", (void *)&b.b - (void *)&b.a); // B: &b-&a=4
}
