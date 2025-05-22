//           main.c - Main function: runs shell to load executable
//          

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

//           end of kernel image (defined in kernel.ld)
extern char _kimg_end[];
struct io_lit lit;
extern char _companion_f_start[];
extern char _companion_f_end[];

#define RAM_SIZE (8*1024*1024)
#define RAM_START 0x80000000UL
#define KERN_START RAM_START
#define USER_START 0x80100000UL

#define UART0_IOBASE 0x10000000
#define UART1_IOBASE 0x10000100
#define UART0_IRQNO 10

#define VIRT0_IOBASE 0x10001000
#define VIRT1_IOBASE 0x10002000
#define VIRT0_IRQNO 1

#define BUFLEN 2*4096

// function declarations
static void shell_main(struct io_intf * termio);
void test_ioctl_command(struct io_intf *testio, int cmd, int *value, const char *cmd_name);
void run_ioctl_tests(struct io_intf *testio);


void main(void) {
    struct io_intf * termio;
    struct io_intf * blkio;
    void * mmio_base;
    int result;
    int i;

    console_init();
    intr_init();
    devmgr_init();
    thread_init();
    timer_init();
    fs_init();

    heap_init(_kimg_end, (void*)USER_START);

    //           Attach NS16550a serial devices

    for (i = 0; i < 2; i++) {
        mmio_base = (void*)UART0_IOBASE;
        mmio_base += (UART1_IOBASE-UART0_IOBASE)*i;
        uart_attach(mmio_base, UART0_IRQNO+i);
    }
    
    //           Attach virtio devices

    // for (i = 0; i < 8; i++) {
    //     mmio_base = (void*)VIRT0_IOBASE;
    //     mmio_base += (VIRT1_IOBASE-VIRT0_IOBASE)*i;
    //     virtio_attach(mmio_base, VIRT0_IRQNO+i);
    // }

    intr_enable();
    timer_start();

    // use the iolit to test
    blkio = iolit_init(&lit, _companion_f_start, _companion_f_end-_companion_f_start);
    result = fs_mount(blkio);
    kprintf("Mounted blk0 (iolit)\n");
    if (result != 0)
        panic("fs_mount failed\n");

    // result = device_open(&blkio, "blk", 0);

    // if (result != 0)
    //     panic("device_open failed");
    
    // result = fs_mount(blkio);

    debug("Mounted blk0");

    if (result != 0)
        panic("fs_mount failed");

    //           Open terminal for trek

    result = device_open(&termio, "ser", 1);

    if (result != 0)
        panic("Could not open ser1");
    kprintf("ser1 Opened\n");
    
    shell_main(termio);
}

void shell_main(struct io_intf * termio_raw) {
    struct io_term ioterm;
    struct io_intf * termio;
    // void (*exe_entry)(struct io_intf*);
    // struct io_intf * exeio;
    char cmdbuf[9];
    // int tid;
    int result;

    termio = ioterm_init(&ioterm, termio_raw);

    char buf[BUFLEN];
    struct io_intf * testio;

    ioputs(termio, "Enter executable name or \"exit\" to exit");
    

    for (;;) {
        ioprintf(termio, "CMD> ");
        ioterm_getsn(&ioterm, cmdbuf, sizeof(cmdbuf));

        if (cmdbuf[0] == '\0')
            continue;

        if (strcmp("exit", cmdbuf) == 0)
            return;
        
        // test fs_open
        if (strcmp("t_open", cmdbuf) == 0){
            kprintf("opening hello...");
            result = fs_open("hello", &testio);
            if (result != 0)
                kprintf("fs_open for existing file failed\n");
            else{
                kprintf("opening hello success\n");
                ioclose(testio);
                kprintf("opening a non-existing file...\n");
                result = fs_open("non-existing",&testio);
                if (result == -ENOENT){
                    kprintf("opening a non-existing file failed as expected\n");
                    kprintf("fs_open test success\n");
                }
                else
                    kprintf("fs_open for non-existing file failed\n");
            }
            continue;
        }

        // test fs_read
        if (strcmp("t_read",cmdbuf) == 0){
            // test read in file
            kprintf("test read for small content\n");
            result = fs_open("t_read", &testio);
            if (result != 0)
                kprintf("fs_open failed\n");
            else{
                ioseek(testio, 0);
                result = ioread(testio, buf, 42);  // number 42 for test
                if (result != 42)
                    kprintf("fs_read in file failed\n");
                else{
                    kprintf("fs_read for small content success\n");
                    kprintf("Content in file: %s\n", buf);
                }
                ioclose(testio);
                memset(buf,0,sizeof(buf));
            }

            // test read large file
            kprintf("test read for large content\n");
            result = fs_open("t_read", &testio);
            if (result != 0)
                kprintf("fs_open failed\n");
            else{
                result = ioseek(testio, 0);
                if (result == -EINVAL){
                    kprintf("set position error\n");
                }
                result = ioread(testio, buf, 6000);  // number 6000 for test
                if (result == -EIO)
                    kprintf("fs_read for large content error\n");
                else if (result == 4523){
                    kprintf("testing fs_read for large content success\n");
                    kprintf("Content in file: %s\n", buf);
                }
                else{
                    kprintf("testing fs_read for large content failed\n");
                }
                ioclose(testio);
                memset(buf,0,sizeof(buf));
            }

            // test read end of file
            kprintf("test read from end of file\n");
            result = fs_open("t_read", &testio);
            if (result != 0)
                kprintf("fs_open failed\n");
            else{
                kprintf("set position to 4545, should fail\n");
                result = ioseek(testio, 4545);
                if (result == -EINVAL){
                    kprintf("set position error! as expected\n");
                }
                result = ioseek(testio, 4523);
                if (result == -EINVAL){
                    kprintf("set position error!! unexpected\n");
                }
                result = ioread(testio, buf, 6000);  // number 6000 for test
                if (result == -EIO)
                    kprintf("fs_read end of file error\n");
                else if (result == 0){
                    kprintf("testing fs_read read from EOF success\n");
                }
                else{
                    kprintf("testing fs_read end of file failed\n");
                }
                ioclose(testio);
                memset(buf,0,sizeof(buf));
            }
            //memset(cmdbuf,0,sizeof(cmdbuf));
            continue;
        }

        // test fs_write
        if (strcmp("t_write", cmdbuf) == 0){
            char writebuf[10] = "I see you ";
            result = fs_open("to_write", &testio);
            if (result != 0)
                kprintf("fs_open failed\n");
            else{
                ioseek(testio, 0);
                result = ioread(testio, buf, 20);  // number 20 for test
                if(result != 20){
                    kprintf("fs_read failed\n");
                }
                else{
                    kprintf("Content before write:%s\n",buf);
                    ioseek(testio, 0);
                    result = iowrite(testio, writebuf, 10);
                    if(result != 10){
                        kprintf("fs_write failed\n");
                    }
                    else{
                        kprintf("fs_write succeeded\n");
                        ioseek(testio, 0);
                        result = ioread(testio, buf, 20);
                        if(result != 20){
                            kprintf("fs_read failed\n");
                        }
                        else{
                            kprintf("Content after write:%s\n",buf);
                        }
                    }
                }
            }
            ioclose(testio);
            memset(buf,0,sizeof(buf));
            continue;
        }

        // test ioctl
        if (strcmp("t_ioctl", cmdbuf) == 0){
            result = fs_open("t_read", &testio);
            if (result != 0)
                kprintf("fs_open failed\n");
            else {
                run_ioctl_tests(testio);
            }
        }
        

        
        // result = fs_open(cmdbuf, &exeio);

        // if (result < 0) {
        //     if (result == -ENOENT)
        //         kprintf("%s: File not found\n", cmdbuf);
        //     else
        //         kprintf("%s: Error %d\n", cmdbuf, -result);
        //     continue;
        // }

        // debug("Calling elf_load(\"%s\")", cmdbuf);

        // result = elf_load(exeio, &exe_entry);

        // debug("elf_load(\"%s\") returned %d", cmdbuf, result);

        // if (result < 0) {
        //     kprintf("%s: Error %d\n", -result);
        
        // } else {
        //     tid = thread_spawn(cmdbuf, (void*)exe_entry, termio_raw);

        //     if (tid < 0)
        //         kprintf("%s: Error %d\n", -result);
        //     else
        //         thread_join(tid);
        // }

        // ioclose(exeio);
    }
}


void test_ioctl_command(struct io_intf *testio, int cmd, int *value, const char *cmd_name) {
    int result = testio->ops->ctl(testio, cmd, value);
    if (result != 0) {
        kprintf("%s failed with code %d\n", cmd_name, result);
    } else {
        kprintf("%s success - value = %d\n", cmd_name, *value);
    }
}

void run_ioctl_tests(struct io_intf *testio) {
    int pos = 0;
    kprintf("Testing IOCTL commands\n");

    // Test IOCTL_GETBLKSZ
    test_ioctl_command(testio, IOCTL_GETBLKSZ, &pos, "IOCTL_GETBLKSZ");
    int expected_blk_size = DATABLKSIZE;
    if (pos != expected_blk_size) {
        kprintf("IOCTL_GETBLKSZ returned unexpected block size: %d (expected %d)\n", pos, expected_blk_size);
    }
    else {
        kprintf("IOCTL_GETBLKSZ returned expected block size:%d\n", pos);
    }

    // Test IOCTL_GETLEN
    test_ioctl_command(testio, IOCTL_GETLEN, &pos, "IOCTL_GETLEN");

    // Test IOCTL_GETPOS
    test_ioctl_command(testio, IOCTL_GETPOS, &pos, "IOCTL_GETPOS");

    // Test IOCTL_SETPOS
    kprintf("Seeking to 3000\n");
    int result = ioseek(testio, 3000);
    if (result != 0) {
        kprintf("ioseek to 3000 failed\n");
    } else {
        test_ioctl_command(testio, IOCTL_GETPOS, &pos, "IOCTL_GETPOS after seeking to 3000");
    }

    // Move to an end-of-bounds position and verify EOF handling
    kprintf("Seeking to 4523 - should be at EOF 4523 and should not fail\n");
    result = ioseek(testio, 4523);
    if (result != 0) {
        kprintf("ioseek to 4523 failed\n");
    } else {
        test_ioctl_command(testio, IOCTL_GETPOS, &pos, "IOCTL_GETPOS after seeking end-of-bounds");
    }

    // Test IOCTL_SETLEN (Expected to fail since the function is not implemented)
    pos = 1000; // Example length, replace if specific length is required
    result = testio->ops->ctl(testio, IOCTL_SETLEN, &pos);
    if (result != 0) {
        kprintf("IOCTL_SETLEN failed as expected\n");
    } else {
        kprintf("IOCTL_SETLEN unexpectedly succeeded\n");
    }
}
