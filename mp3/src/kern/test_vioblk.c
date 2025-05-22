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

//            end of kernel image (defined in kernel.ld)
extern char _kimg_end[];

#define RAM_SIZE (8 * 1024 * 1024)
#define RAM_START 0x80000000UL
#define KERN_START RAM_START
#define USER_START 0x80100000UL

#define UART0_IOBASE 0x10000000
#define UART1_IOBASE 0x10000100
#define UART0_IRQNO 10

#define VIRT0_IOBASE 0x10001000
#define VIRT1_IOBASE 0x10002000
#define VIRT0_IRQNO 1

/**
 * @brief Main entry function for initializing system components and testing VirtIO block device functionality.
 * 
 * This function initializes essential system components and performs a series of tests on the VirtIO 
 * block device interface, including device attachment, I/O operations, and validation of control commands.
 * 
 * @details 
 * - **Device Attachment**: Attaches up to eight VirtIO devices.
 * - **Device Testing**:
 *   - Tests `device_open` to verify device access.
 *   - Tests control commands (`IOCTL_GETBLKSZ`, `IOCTL_GETLEN`, `IOCTL_GETPOS`, `IOCTL_SETPOS`) for block size, device length, and position management.
 *   - Performs read and write operations, validates data consistency, and tests boundary conditions for reading and writing beyond device length.
 * 
 * @return void
 */
void main(void)
{
    struct io_intf *blkio;
    void *mmio_base;
    int result;
    int i;

    console_init();
    intr_init();
    devmgr_init();
    thread_init();
    timer_init();

    heap_init(_kimg_end, (void *)USER_START);

    //            Attach NS16550a serial devices
    for (i = 0; i < 2; i++)
    {
        mmio_base = (void *)UART0_IOBASE;
        mmio_base += (UART1_IOBASE - UART0_IOBASE) * i;
        uart_attach(mmio_base, UART0_IRQNO + i);
    }

    //            Attach virtio devices
    for (i = 0; i < 8; i++)
    {
        mmio_base = (void *)VIRT0_IOBASE;
        mmio_base += (VIRT1_IOBASE - VIRT0_IOBASE) * i;
        virtio_attach(mmio_base, VIRT0_IRQNO + i);
    }

    console_printf("Completed virtio_attach tests\n\n");

    intr_enable();
    timer_start();

    // test device_open
    result = device_open(&blkio, "blk", 0);
    if (result != 0)
        panic("device_open failed");
    else
        console_printf("device_open succeeded\n");

    // test open invalid device
    struct io_intf *invalid_io;
    result = device_open(&invalid_io, "nonexistent", 0);
    if (result == 0)
        console_printf("Error: Opened nonexistent device\n");
    else
        console_printf("Correctly failed to open nonexistent device\nCompleted device_open tests\n\n");

    // test ctl four cmd
    uint64_t len = 2000;
    uint64_t pos = 0;
    uint64_t blksz = 512; //512 for the size
    uint64_t new_pos = 100;

    // get the block size
    result = blkio->ops->ctl(blkio, IOCTL_GETBLKSZ, &blksz);
    if (result != 0)
        console_printf("IOCTL_GETBLKSZ failed\n");
    else
        console_printf("Block size: %lu\n", blksz);

    // det the length of the device
    result = blkio->ops->ctl(blkio, IOCTL_GETLEN, &len);
    if (result != 0)
        console_printf("IOCTL_GETLEN failed\n");
    else
        console_printf("Device length: %lu\n", len);

    // get the recent position
    result = blkio->ops->ctl(blkio, IOCTL_GETPOS, &pos);
    if (result != 0)
        console_printf("IOCTL_GETPOS failed\n");
    else
        console_printf("Current position: %lu\n", pos);

    // set the new position
    new_pos = blksz * 2; // set to offset blksz * 2
    result = blkio->ops->ctl(blkio, IOCTL_SETPOS, &new_pos);
    if (result != 0)
        console_printf("IOCTL_SETPOS failed\n");
    else
        console_printf("Set position to: %lu\n", new_pos);

    // valid the position
    result = blkio->ops->ctl(blkio, IOCTL_GETPOS, &pos);
    if (result != 0)
        console_printf("IOCTL_GETPOS failed\n");
    else
        console_printf("Current position after set: %lu\n", pos);
    
    console_printf("Completed four ctl function test\n\n");

    // test read and write
    char *write_buf = kmalloc(blksz);
    memset(write_buf, 'A', blksz);

    result = blkio->ops->write(blkio, write_buf, blksz);
    if (result != blksz)
        console_printf("Write failed or incomplete\n");
    else
        console_printf("Write successful\nCompleted vioblk_write tests\n\n");

    // reset the position to read the data
    result = blkio->ops->ctl(blkio, IOCTL_SETPOS, &new_pos);

    char *read_buf = kmalloc(blksz);
    result = blkio->ops->read(blkio, read_buf, blksz);
    if (result != blksz)
        console_printf("Read failed or incomplete\n");
    else
        console_printf("Read successful\nCompleted vioblk_read tests\n\n");

    // valid if the data consistency
    if (memcmp(write_buf, read_buf, blksz) == 0)
        console_printf("Data verification successful\nRead and Write data is consistence\n\n");
    else
        console_printf("Data verification failed\n");

    kfree(write_buf);
    kfree(read_buf);

    // write out of the device size
    new_pos = len - blksz / 2;
    result = blkio->ops->ctl(blkio, IOCTL_SETPOS, &new_pos);
    if (result != 0)
        console_printf("IOCTL_SETPOS failed\n");
    else
        console_printf("Set position to: %lu\n", new_pos);

    write_buf = kmalloc(blksz);
    memset(write_buf, 'B', blksz);
    result = blkio->ops->write(blkio, write_buf, blksz);
    if (result != blksz)
        console_printf("Write beyond device length failed as expected\n");
    else
        console_printf("Error: Write beyond device length succeeded\n");

    kfree(write_buf);

    // test read out of the device size
    read_buf = kmalloc(blksz);
    result = blkio->ops->read(blkio, read_buf, blksz);
    if (result != blksz)
        console_printf("Read beyond device length failed as expected\n");
    else
        console_printf("Error: Read beyond device length succeeded\n");

    console_printf("Completed test read and write out of range situation\n\n");
    kfree(read_buf);

    console_printf("All tests completed\n");

}
