// process.c - user process
//

#ifdef PROCESS_TRACE
#define TRACE
#endif

#ifdef PROCESS_DEBUG
#define DEBUG
#endif

#include "process.h"
#include "memory.h"
#include "thread.h"
#include "io.h"
#include "csr.h"
#include "intr.h"
#include "error.h"
#include "halt.h"
#include "elf.h"
#include "heap.h"

// COMPILE-TIME PARAMETERS
//

// NPROC is the maximum number of processes

#ifndef NPROC
#define NPROC 16
#endif

// INTERNAL FUNCTION DECLARATIONS
//

// INTERNAL GLOBAL VARIABLES
//

#define MAIN_PID 0

#define GET_SATP_ASID(satp) (((satp) >> 44) & 0x0FFFF)

// The main user process struct

static struct process main_proc;

// The process content table
int process_count = 1;

// A table of pointers to all user processes in the system

struct process * proctab[NPROC] = {
    [MAIN_PID] = &main_proc
};

// EXPORTED GLOBAL VARIABLES
//

char procmgr_initialized = 0;

// EXPORTED FUNCTION DEFINITIONS
//

/**
 * Initializes the process manager. Sets up the main process and prepares the process table.
 * 
 * Input -- None
 * 
 * Return -- None
 * 
 * This function initializes the main process manager. It sets up the main process (`main_proc`), 
 * assigns its process ID (`MAIN_PID`), and links it to the currently running thread. 
 * Additionally, it initializes the process's I/O interface table and marks
 * the process manager as initialized.
 */
extern void procmgr_init(void){
    // Check if the manager is already initialized
    if(procmgr_initialized) return;

    main_proc.id = MAIN_PID;  // Set the process id
    main_proc.tid = running_thread();  // Set the thread id
    main_proc.mtag = active_memory_space();  // Set the memory space identifier
    thread_set_process(main_proc.tid, &main_proc);

    // Set the I/O interface table of the process
    for (int i = 0; i < PROCESS_IOMAX; i++){
        main_proc.iotab[i] = NULL;
    }

    // Set the initialization label
    procmgr_initialized = 1;
}


/**
 * Jumps to the User mode and executes a new user process.
 * 
 * Input -- exeio: The I/O interface referring to the executable.
 * 
 * Return -- 0 if success;
 *        -- Negative value if failure;
 * 
 * This function replaces the current process's memory with the executable specified by the given I/O interface. 
 * It unmaps all current user-space memory, loads the executable using `elf_load`,
 * and then transitions the thread to user mode to execute the new process.
 */
extern int process_exec(struct io_intf * exeio){
    // Check if the I/O interface is available
    if(exeio == NULL){
        return -EIO;
    }

    void (*entry)(void);  // Create an entry pointer of the executable file

    struct process* curr_proc = current_process();  // Get the current process
    if(curr_proc == NULL){
        panic("Failed to get current process.");
    }
    thread_set_process(curr_proc->tid, curr_proc);

    // I. Unmap all pages mapped into user address space and frees the backing physical pages
    memory_unmap_and_free_user();
    curr_proc->mtag = active_memory_space();

    // // II. Create new memory space
    // uintptr_t new_mtag = memory_space_create(curr_proc->id);
    // curr_proc->mtag = new_mtag;

    // memory_space_switch(new_mtag);  // Switch memory space

    // II. Load the executable from the I/O interface
    int result = elf_load(exeio, &entry);
    if(result < 0){
        return result;
    }

    // III. Jump to User Mode and start the thread
    intr_disable();  // Disable interrupt
    // Fill in the values of bits SPP and SPIE
    csrc_sstatus(RISCV_SSTATUS_SPP); // SPP Bit
    csrs_sstatus(RISCV_SSTATUS_SPIE); // SPIE Bit

    thread_jump_to_user((uintptr_t)USER_STACK_VMA,(uintptr_t)entry);
}

/**
 * Forks a child process from the current process.
 * 
 * Input -- tfr: The trap frame of the parent process.
 * 
 * Return -- The process ID of the child process if success;
 *        -- Negative value if failure;
 * 
 * This function creates a new user process by forking the current process. 
 * It clones the current process's memory space, I/O interface table, and thread, 
 * and then sets up the child process with the new memory space and thread. 
 * The function returns the process ID of the child process if successful.

*/
extern int process_fork(const struct trap_frame * tfr){
    // Get the current process
    struct process* curr_proc = current_process();
    if(curr_proc == NULL){
        panic("Failed to get current process.");
    }

    // Create a new process
    // if the process count is greater than or equal to the maximum number of processes
    if (process_count >= NPROC){
        return -EINVAL; //there are too many processes
    }

    int id;
    for(id = 0; id < NPROC && proctab[id] != NULL; id++);
    if(id == NPROC) panic("There's too many processes.");

    struct process* new_proc = kmalloc(sizeof(struct process));
    if(new_proc == NULL){
        return -12; // ENOMEM for no memory ENOMEM = 12
    }

    // Set the new process id and insert it into the process table
    new_proc->id = id;
    proctab[id] = new_proc;

    //Copy the memory space
    // uintptr_t copied_mtag = memory_space_clone(0);
    // new_proc->mtag = copied_mtag;

    // // Copy the I/O interface table
    // for (int i = 0; i < PROCESS_IOMAX; ++i){
    //     if(curr_proc->iotab[i] != NULL){
    //         new_proc->iotab[i] = curr_proc->iotab[i];
    //         ioref(curr_proc->iotab[i]);
    //     }
    // }

    // Return the process id of the child process
    return thread_fork_to_user(new_proc, tfr);
}


/**
 * Terminates the currently running user process and releases its resources.
 * 
 * Input -- None.
 * 
 * Return -- None.
 * 
 * This function reclaims the current process's memory space, closes any open I/O interfaces, 
 * removes the process from the global process table, and deallocates its resources. 
 * It also unassigns the thread associated with the process and terminates the thread.
 */
extern void __attribute__ ((noreturn)) process_exit(void){
    kprintf("Process_exit is called.\n");

    // Get the process
    struct process* proc = current_process();
    if(proc == NULL){
        panic("Failed to get current process.");
    }
    kprintf("here the %d process is exited\n", proc->id);
    // Get the thread id of the process
    int tid = proc->tid;

    // Release with the items listed below:
    // I. Reclaim process memory space
    memory_space_reclaim();

    // II. Close I/O interfaces
    for(int j = 0; j < PROCESS_IOMAX; j++){
        if(proc->iotab[j] != NULL){
            ioclose(proc->iotab[j]);
            proc->iotab[j] = NULL;
        }
    }

    // III. Remove the process and free it
    if(proc->id != MAIN_PID){
        proctab[proc->id] = NULL;
        kfree(proc);
    }

    // Set the associated process of the thread to none
    thread_set_process(tid, NULL);
    // Exit from the thread
    thread_exit();
}


/**
 * Terminates a user process by its process ID and releases its resources.
 * 
 * Input -- pid: The process ID of the process to terminate.
 * 
 * Return -- None.
 * 
 * This function locates the process in the process table by its ID (`pid`). 
 * It reclaims its memory space, closes its I/O interfaces, removes it from the global process table, 
 * deallocates its resources, and unassigns the thread associated with the process. 
 */
extern void process_terminate(int pid){
    // Check if the pid is available
    if(pid < 0 || pid >= NPROC || proctab[pid] == NULL){
        return;
    }

    // Get the process
    struct process* proc = proctab[pid];
    // Get the thread id of the process
    int tid = proc->tid;

    // Release with the items listed below:
    // I. Process memory space
    memory_space_reclaim();

    // II. Open I/O interfaces
    for(int j = 0; j < PROCESS_IOMAX; j++){
        if(proc->iotab[j] != NULL){
            ioclose(proc->iotab[j]);
            proc->iotab[j] = NULL;
        }
    }

    // III. Remove the process and free it
    if(proc->id != MAIN_PID){
        proctab[proc->id] = NULL;
        kfree(proc);
    }

    // Set the associated process of the thread to none
    thread_set_process(tid, NULL);
}