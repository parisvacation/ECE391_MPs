#include "error.h"
#include "process.h"
#include "io.h"
#include "device.h"
#include "fs.h"
#include "memory.h"
#include "thread.h"
#include "console.h"
#include "halt.h"
#include "trap.h"
#include "scnum.h"
#include "timer.h"
#include "heap.h"
#include "intr.h"
#include "syscall.h"

#define ECHILD  10
#define NPROC 16


static int sysexit(void) {
    // exit the current process
    kprintf("exit has been used\n");
    process_exit();
}

static int sysmsgout(const char *msg) {
    int result;

    result = memory_validate_vstr(msg, PTE_U);
    // check if the message is valid，0 is success
    if (result != 1) {
        console_printf("sysmsgout refuse the request\n");
        return result;
    }
        

    console_printf("Thread <%s:%d> says: %s\n",
            thread_name(running_thread()),
            running_thread(),
            msg);

    // return 0 on success
    return 0;
}

static int sysdevopen(int fd, const char *name, int instno) {
    int result = memory_validate_vstr(name, PTE_U);
    // check if the name is valid，0 is success
    if (result != 1)
        return result;

    // validate the file descriptor
    if (fd >= 0) {
        // check if the file descriptor is valid
        if(fd >= PROCESS_IOMAX) {
            return -EBADFD;
        }
        int new_fd=fd;

        if(current_process()->iotab[new_fd] != NULL) return -EBUSY;
        
        struct io_intf *io;
        result = device_open(&io, name, instno);
        if(result < 0){
            return result;
        }
        current_process()->iotab[new_fd] = io;  
        return fd;
    }
    else {
        int new_fd;
        //find the next available file descriptor
        for(new_fd = 0; new_fd < PROCESS_IOMAX; new_fd++) {
            if(current_process()->iotab[new_fd] == NULL) {
                break;
            }
        }
        if(current_process()->iotab[new_fd] != NULL) return -EBUSY;
        struct io_intf *io;
        result = device_open(&io, name, instno);
        if(result < 0){
            return result;
        }
        current_process()->iotab[new_fd] = io;  
        return fd;
    }
}


int sysfsopen(int fd, const char *name) {
    int result = memory_validate_vstr(name, PTE_U);
    // check if the name is valid，0 is success
    if (result != 1)
        return result;

    // validate the file descriptor
    if (fd >= 0) {
        // check if the file descriptor is valid
        if(fd >= PROCESS_IOMAX) {
            return -EBADFD;
        }
        int new_fd=fd;

        if(current_process()->iotab[new_fd] != NULL) return -EBUSY;
        
        struct io_intf *io;
        result = fs_open(name, &io);
        if(result < 0){
            return result;
        }
        current_process()->iotab[new_fd] = io;  
        return fd;
    }
    else {
        int new_fd;
        //find the next available file descriptor
        for(new_fd = 0; new_fd < PROCESS_IOMAX; new_fd++) {
            if(current_process()->iotab[new_fd] == NULL) {
                break;
            }
        }
        if(current_process()->iotab[new_fd] != NULL) return -EBUSY;
        struct io_intf *io;
        result = fs_open(name,&io);
        if(result < 0){
            return result;
        }
        current_process()->iotab[new_fd] = io;  
        return fd;
    }
}

static int sysclose(int fd) {
    // validate the file descriptor
    if (fd >= 0 && fd < PROCESS_IOMAX) {
        // check if the file descriptor is valid
        if(current_process()->iotab[fd] == NULL) {
            return -EBADFD;
        }
        // close the file descriptor
        ioclose(current_process()->iotab[fd]);
        current_process()->iotab[fd] = NULL;
        return 0;
    }
    else {
        return -EBADFD;
    }
}

static long sysread(int fd, void *buf, size_t bufsz) {
    // Memory Range Validation
    int validate_result;

    validate_result = memory_validate_vptr_len(buf, bufsz, PTE_W|PTE_U);
    if(validate_result != 1) {
        return validate_result;
    }

    // validate the file descriptor
    if (fd >= 0 && fd < PROCESS_IOMAX) {
        // check if the file descriptor is valid
        if(current_process()->iotab[fd] == NULL) {
            return -EINVAL;
        }
        // return the result of device_read
        else return ioread_full(current_process()->iotab[fd], buf, bufsz);
    }
    else {
        return -EBADFD;
    }
}

static long syswrite(int fd, const void *buf, size_t len) {
    // Memory Range Validation
    int validate_result;

    validate_result = memory_validate_vptr_len(buf, len, PTE_R |PTE_U);
    if(validate_result != 1) {
        return validate_result;
    }

    // validate the file descriptor
    if (fd >= 0 && fd < PROCESS_IOMAX) {
        // check if the file descriptor is valid
        if(current_process()->iotab[fd] == NULL) {
            return -EINVAL;
        }
        // return the result of device_write
        return iowrite(current_process()->iotab[fd], buf, len);
    }
    else {
        return -EBADFD;
    }
}

static int sysioctl(int fd, int cmd, void *arg) {
    // validate the file descriptor
    if (fd >= 0 && fd < PROCESS_IOMAX) {
        // check if the file descriptor is valid
        if(current_process()->iotab[fd] == NULL) {
            return -EINVAL;
        }
        // return the result of device_ioctl
        return ioctl(current_process()->iotab[fd], cmd, arg);
    }
    else {
        return -EBADFD;
    }
}

static int sysexec(int fd) {
    // validate the file descriptor
    if (fd >= 0 && fd < PROCESS_IOMAX) {
        // check if the file descriptor is valid
        if(current_process()->iotab[fd] == NULL) {
            return -EINVAL;
        }
        // return the result of process_exec
        process_exec(current_process()->iotab[fd]);
        return -EBADFD;
    }
    else {
        return -EBADFD;
    }
}

static int sysfork(const struct trap_frame *tfr){
    // fork the current process
    return process_fork(tfr);
}

extern void syscall_handler(struct trap_frame *tfr) {
    // // Memory Range Validation
    // int validate_result;

    // validate_result = memory_validate_vptr_len(tfr, sizeof(struct trap_frame), PTE_R |PTE_U);
    // if(validate_result != 0) {
    //     return;
    // }

    tfr->x[TFR_A0]= syscall(tfr);
}

uint64_t syscall(struct trap_frame *tfr) {
    tfr->sepc += 4;
    const uint64_t * const a = tfr->x+ TFR_A0;

    // switch statement to handle the system call
    switch (a[7]) {     // a[7] is the system call number

        // here a[0], a[1], a[2] are the arguments to the system call
        case SYSCALL_MSGOUT:
            return sysmsgout((const char *)a[0]);
            break;
        
        case SYSCALL_DEVOPEN:
            return sysdevopen((int)a[0], (const char *)a[1],(int) a[2]);
            break;

        case SYSCALL_FSOPEN:
            return sysfsopen((int)a[0], (const char *)a[1]);
            break;
        
        case SYSCALL_CLOSE:
            return sysclose((int)a[0]);
            break;

        case SYSCALL_READ:
            return sysread((int) a[0], (void *)a[1],(size_t) a[2]);
            break;
        
        case SYSCALL_WRITE:
            return syswrite((int) a[0], (const void *)a[1],(size_t) a[2]);
            break;
        
        case SYSCALL_IOCTL:
            return sysioctl((int) a[0], (int)a[1], (void *)a[2]);
            break;

        case SYSCALL_EXEC:
            return sysexec((int) a[0]);
            break;
        
        case SYSCALL_WAIT:
            return syswait((int) a[0]);
            break;
        
        case SYSCALL_USLEEP:
            return sysusleep((unsigned long) a[0]);
            break;

        case SYSCALL_EXIT:
            sysexit();
            return 0;
            break;
        
        case SYSCALL_FORK:
            return sysfork(tfr);
            break;

        default :
            return -ENOTSUP;
    };
}


static int syswait(int tid){
    trace("%s(%d)", __func__, tid);

    int result;
    // If the thread is main thread
    if(tid == 0){
        result = thread_join_any();
        return result;
    }

    // If the thread is not main thread
    else{
        result = thread_join(tid);
        return result;
    }
}


static int sysusleep(unsigned long us){
    // Check if the time is zero
    if(us == 0){
        return 0;
    }
    
    // Declare an alarm
    struct alarm al;
    
    // Initialize the alarm
    alarm_init(&al, "sysusleep_alarm");

    // Sleep the thread
    alarm_sleep_us(&al, us);

    return 0;
}