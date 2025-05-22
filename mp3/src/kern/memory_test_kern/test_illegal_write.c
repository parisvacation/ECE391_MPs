//

#ifdef MAIN_TRACE
#define TRACE
#endif

#ifdef MAIN_DEBUG
#define DEBUG
#endif

#define INIT_PROC "test_illegal_write" // name of init process executable

#include "console.h"
#include "thread.h"
#include "device.h"
#include "uart.h"
#include "timer.h"
#include "intr.h"
#include "memory.h"
#include "heap.h"
#include "virtio.h"
#include "halt.h"
#include "elf.h"
#include "fs.h"
#include "string.h"
#include "process.h"
#include "config.h"

void main(void) {
    // initialization
    console_init();
    memory_init();

    // initialize a str
    char* hello = memory_alloc_and_map_page(0xc0018000, PTE_R | PTE_W);

    strncpy(hello, "Hello, World from gmzOS!\n", 25);
    kprintf("message <%s>, at %s", hello, hello);
    kprintf("Now the page is set unwritable.\n");
    kprintf("Expected output: Store page fault\n");
    memory_set_page_flags(hello, 0);
    strncpy(hello, "See you!\n", 9);

}
