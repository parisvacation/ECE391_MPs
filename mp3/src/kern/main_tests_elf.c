#include "console.h"
#include "thread.h"
#include "device.h"
#include "uart.h"
#include "timer.h"
#include "intr.h"
#include "heap.h"
#include "virtio.h"
#include "halt.h"
#include "elf.h"
#include "fs.h"
#include "string.h"

#include <stddef.h>
#include <stdint.h>

extern char _companion_f_start[];
extern char _companion_f_end[];
extern char _kimg_end[];

#define USER_START 0x80100000UL

#define UART0_IOBASE 0x10000000
#define UART1_IOBASE 0x10000100
#define UART0_IRQNO 10

// Define program entry point function pointer
void (*entry_point)(struct io_intf *) = NULL;
// static uint64_t current_offset = 0;

// memory_read function to read data from in-memory file
/*
long memory_read(struct io_intf* io, void* buf, unsigned long bufsz) {
    char* companion_data = _companion_f_start;
    size_t companion_size = _companion_f_end - _companion_f_start;

    // Check if the read range exceeds file size
    if (current_offset + bufsz > companion_size) {
        return -1;
    }

    // Copy bufsz bytes from file start + offset into buf
    memcpy(buf, companion_data + current_offset, bufsz);
    current_offset += bufsz;
    return bufsz;
}

// memory_ctl function to set the current read position
int memory_ctl(struct io_intf* io, int cmd, void* arg) {
    if (cmd == IOCTL_SETPOS) {
        uint64_t new_offset = *(uint64_t*)arg;
        size_t companion_size = _companion_f_end - _companion_f_start;

        if (new_offset > companion_size) {
            return -1;
        }

        current_offset = new_offset;
        return 0;
    }
    return -1;
}
*/

/**
 * test_elf_load - Tests the loading of an ELF executable and starts a thread with the entry point.
 * 
 * Inputs: None
 * 
 * Description:
 * This function initializes the system environment, including console, interrupt, device manager, threading, timer, and heap memory.
 * It then attaches UART devices for serial communication and sets up a file I/O interface to load an ELF file from memory.
 * 
 * The ELF file data is obtained from predefined memory locations `_companion_f_start`
 * and `_companion_f_end`. Using the `elf_load` function, it attempts to load the ELF
 * file into memory and retrieve its entry point. If loading is successful, the function
 * spawns a new thread to execute the loaded ELF program starting at the entry point.
 *
 * Returns:
 * 0 -- Success (ELF file loaded and thread spawned successfully).
 * Non-zero error code -- If loading fails or unable to open the UART device.
 */
int test_elf_load() {
    console_init();
    intr_init();
    devmgr_init();
    thread_init();
    timer_init();

    // Initialize heap memory
    heap_init(_kimg_end, (void*)USER_START);

    // Attach NS16550a serial devices
    void * mmio_base;
    for (size_t i = 0; i < 2; i++) {
        mmio_base = (void*)UART0_IOBASE;
        mmio_base += (UART1_IOBASE-UART0_IOBASE)*i;
        uart_attach(mmio_base, UART0_IRQNO+i);
    }

    // Set up file I/O operations for reading
    /* static const struct io_ops file_ops = {
        .read = memory_read,
        .write = NULL,
        .ctl = memory_ctl,
        .close = NULL
    }; */

    // struct io_intf file_io = { .ops = &file_ops };

    struct io_lit elf_lit;
    struct io_intf * elf_io;

    void * buf = _companion_f_start;
    size_t size = _companion_f_end - _companion_f_start;

    elf_io = iolit_init(&elf_lit, buf, size);

    int result;

    intr_enable();
    timer_start();

    // Open the UART device using device_open to get the terminal I/O interface
    struct io_intf* termio_raw;
    result = device_open(&termio_raw, "ser", 1);  // Use device_open to initialize UART as the base I/O
    if (result != 0)
        panic("Could not open ser1");

    // struct io_term termio;
    // struct io_intf* terminal_io = ioterm_init(&termio, termio_raw);  // Initialize ioterm with termio_raw

    result = elf_load(elf_io, &entry_point);
    if (result == 0 && entry_point != NULL) {
        console_puts("SUCCESS!");

        int tid = thread_spawn("test", (void*)entry_point, termio_raw);

        if (tid < 0){
            console_puts("FAILURE");
        }
        else{
            thread_join(tid);
        }
    } else {
        console_puts("FAILURE");
    }

    return result;
}

// Main function
int main() {
    test_elf_load();  // Run the test function
    return 0;
}


/*
// Simple write operation, using uart to output the result
long simple_write(struct io_intf* io, const void* buf, unsigned long bufsz) {
    const char* data = (const char*)buf;
    for (unsigned long i = 0; i < bufsz; i++) {
        com0_putc(data[i]);
    }
    return bufsz;
}

// Test the elf_load function()
int test_elf_load() {
    console_init();
    thread_init();

    // Initialize heap memory
    heap_init(_kimg_end, (void*)USER_START);

    // Set file I/O operations
    static const struct io_ops file_ops = {
        .read = memory_read,
        .write = NULL,
        .ctl = memory_ctl,
        .close = NULL 
    };

    struct io_intf file_io = { .ops = &file_ops };

    // set simple I/O operations
    static const struct io_ops simple_ops = {
        .write = simple_write, 
        .read = NULL,
        .ctl = NULL,
        .close = NULL
    };

    struct io_intf simple_io = { .ops = &simple_ops };

    // Load the elf file into the kernel memory
    int result = elf_load(&file_io, &entry_point);
    if (result == 0 && entry_point != NULL) {
        com0_putc('S');
        com0_putc('U');
        com0_putc('C');
        com0_putc('C');
        com0_putc('E');
        com0_putc('S');
        com0_putc('S');
        com0_putc('\n');

        int tid = thread_spawn("test", (void*)entry_point, &simple_io);  // Execute the loaded program with simple I/O
        if (tid < 0){
            com0_putc('F');
            com0_putc('A');
            com0_putc('I');
            com0_putc('L');
            com0_putc('U');
            com0_putc('R');
            com0_putc('E');
            com0_putc('\n');
        }
        else{
            thread_join(tid);
        }
    } else {
        com0_putc('F');
        com0_putc('A');
        com0_putc('I');
        com0_putc('L');
        com0_putc('U');
        com0_putc('R');
        com0_putc('E');
        com0_putc('\n');
    }

    return result;
}

int main() {
    com0_init();
    test_elf_load(); 
    return 0;
}
*/