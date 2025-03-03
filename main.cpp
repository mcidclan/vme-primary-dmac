// m-c/d 2025
#include "main.h"

PSP_MODULE_INFO("vmegdma", 0, 1, 1);
PSP_HEAP_SIZE_KB(-1024);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_VFPU | PSP_THREAD_ATTR_USER);

static volatile u32* buffer = nullptr;
static volatile u32* mem = nullptr;
#define meCounter         (mem[0])
#define meExit            (mem[1])

__attribute__((noinline, aligned(4)))
static void meLoop() {
  do {
    meDCacheWritebackInvalidAll();
  } while(!mem || !buffer);
  
  memToVme((u32)buffer, 0, 2);
  hw(0x44000000) = (hw(0x44000000) & 0x00ffffff) + 2;
  hw(0x44000004) = (hw(0x44000004) & 0x00ffffff) * 2;
  vmeToMem(0, ((u32)buffer + 0x08), 2);
  // note: 0, used as src and dst, is the base of the first internal dsp/vme buffers with a length of 0x2000 bytes.
  
  do {
    meCounter++;
  } while(meExit == 0);
  meExit = 2;
  meHalt();
}

extern char __start__me_section;
extern char __stop__me_section;
__attribute__((section("_me_section")))
void meHandler() {
  hw(0xbc100050) = 0x6f;
  hw(0xbc100004) = 0xffffffff;
  hw(0xbc100040) = 0x02;
  asm("sync");
  
  asm volatile(
    "li          $k0, 0x30000000\n"
    "mtc0        $k0, $12\n"
    "sync\n"
    "la          $k0, %0\n"
    "li          $k1, 0x80000000\n"
    "or          $k0, $k0, $k1\n"
    "jr          $k0\n"
    "nop\n"
    :
    : "i" (meLoop)
    : "k0"
  );
}

static int initMe() {
  #define me_section_size (&__stop__me_section - &__start__me_section)
  memcpy((void *)ME_HANDLER_BASE, (void*)&__start__me_section, me_section_size);
  meDCacheWritebackInvalidAll();
  hw(0xbc10004c) = 0x04;
  hw(0xbc10004c) = 0x0;
  asm volatile("sync");
  return 0;
}

void exitSample(const char* const str) {
  pspDebugScreenInit();
  pspDebugScreenClear();
  pspDebugScreenSetXY(0, 1);
  pspDebugScreenPrintf(str);
  sceKernelDelayThread(100000);
  sceKernelExitGame();
}

int main() {
  scePowerSetClockFrequency(333, 333, 166);

  if (pspSdkLoadStartModule("ms0:/PSP/GAME/me/kcall.prx", PSP_MEMORY_PARTITION_KERNEL) < 0){
    exitSample("Can't load the PRX, exiting...");
    return 0;
  }

  buffer = (u32*)memalign(16, 4*sizeof(u32));
  buffer[0] = 1;
  buffer[1] = 2;
  sceKernelDcacheWritebackInvalidateAll();
  
  mem = meGetUncached32(4);
  kcall(&initMe);
  
  sceDisplaySetFrameBuf((void*)(UNCACHED_USER_MASK |
    GE_EDRAM_BASE), 512, 3, PSP_DISPLAY_SETBUF_NEXTFRAME);
  pspDebugScreenInitEx(0, PSP_DISPLAY_PIXEL_FORMAT_8888, 0);
  pspDebugScreenSetOffset(0);
  
  SceCtrlData ctl;
  do {
    sceCtrlPeekBufferPositive(&ctl, 1);
    pspDebugScreenSetXY(0, 1);
    pspDebugScreenPrintf("%x          ", meCounter);
    pspDebugScreenSetXY(0, 2);
    pspDebugScreenPrintf("%u          ", 0x00ffffff & *uhw(&(buffer[2])));
    pspDebugScreenSetXY(0, 3);
    pspDebugScreenPrintf("%u          ", 0x00ffffff & *uhw(&(buffer[3])));
    sceDisplayWaitVblankStart();
  } while(!(ctl.Buttons & PSP_CTRL_HOME));
  
  meExit = 1;
  do {
    asm volatile("sync");
  } while(meExit < 2);
  
  free((void*)buffer);
  meGetUncached32(0);
  exitSample("Exiting...");
  return 0;
}
