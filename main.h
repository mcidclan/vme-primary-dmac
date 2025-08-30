// m-c/d 2025
#pragma once
#include <psppower.h>
#include <pspdisplay.h>
#include <pspsdk.h>
#include <pspkernel.h>
#include <pspctrl.h>
#include <cstring>
#include <malloc.h>
#include <cmath>
#include "kcall.h"

#define u8  unsigned char
#define u16 unsigned short int
#define u32 unsigned int

#define hwp          volatile u32*
#define hw(addr)     (*((hwp)(addr)))
#define uhw(addr)    ((u32*)(0x40000000 | ((u32)addr)))

#define ME_EDRAM_BASE         0
#define GE_EDRAM_BASE         0x04000000
#define UNCACHED_USER_MASK    0x40000000
#define CACHED_KERNEL_MASK    0x80000000
#define UNCACHED_KERNEL_MASK  0xA0000000

#define ME_HANDLER_BASE       0xbfc00000

inline void meDcacheWritebackInvalidateAll() {
  asm volatile ("sync");
  for (int i = 0; i < 8192; i += 64) {
    asm("cache 0x14, 0(%0)" :: "r"(i));
    asm("cache 0x14, 0(%0)" :: "r"(i));
  }
  asm volatile ("sync");
}

inline void meHalt() {
  asm volatile(".word 0x70000000");
}

inline void meGetUncached32(volatile u32** const mem, const u32 size) {
  static void* _base = nullptr;
  if (!_base) {
    const u32 byteCount = size * 4;
    _base = memalign(16, byteCount);
    memset(_base, 0, byteCount);
    sceKernelDcacheWritebackInvalidateAll();
    *mem = (u32*)(UNCACHED_USER_MASK | (u32)_base);
    __asm__ volatile (
      "cache 0x1b, 0(%0)  \n"
      "sync               \n"
      : : "r" (mem) : "memory"
    );
    return;
  } else if (!size) {
    free(_base);
  }
  *mem = nullptr;
  return;
}

inline void vmeDmaWait() {
  do {
    asm volatile("sync");
  } while((hw(0x440ff000) & 0x200) != 0);
}

// vme 24 bits buffer to memory
inline void vmeToMem(u32 const src, const u32 dst, const u32 count) {
  hw(0x440ff010) = (dst & 0x1fffffff) | 0x40000000; // src addr
  hw(0x440ff014) = count - 1;                       // count -1
  hw(0x440ff018) = src;                             // dst addr
  hw(0x440ff008) = 0x48;                            // control
  vmeDmaWait();
}

// memory to vme 24 bits buffer
inline void memToVme(u32 const src, const u32 dst, const u32 count) {
  hw(0x440ff010) = (src & 0x1fffffff) | 0x40000000; // src addr
  hw(0x440ff014) = count - 1;                       // count -1
  hw(0x440ff018) = dst;                             // dst addr
  hw(0x440ff008) = 0x40;                            // control
  vmeDmaWait();
}
