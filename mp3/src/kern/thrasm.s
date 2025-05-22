# thrasm.s - Special functions called from thread.c
#

# struct thread * _thread_swtch(struct thread * resuming_thread)

# Switches from the currently running thread to another thread and returns when
# the current thread is scheduled to run again. Argument /resuming_thread/ is
# the thread to be resumed. Returns a pointer to the previously-scheduled
# thread. This function is called in thread.c. The spelling of swtch is
# historic.

        .text
        .global _thread_swtch
        .type   _thread_swtch, @function

_thread_swtch:

        # We only need to save the ra and s0 - s12 registers. Save them on
        # the stack and then save the stack pointer. Our declaration is:
        # 
        #   struct thread * _thread_swtch(struct thread * resuming_thread);
        #
        # The currently running thread is suspended and resuming_thread is
        # restored to execution. swtch returns when execution is switched back
        # to the calling thread. The return value is the previously executing
        # thread. Interrupts are enabled when swtch returns.
        #
        # tp = pointer to struct thread of current thread (to be suspended)
        # a0 = pointer to struct thread of thread to be resumed
        # 

        sd      s0, 0*8(tp)
        sd      s1, 1*8(tp)
        sd      s2, 2*8(tp)
        sd      s3, 3*8(tp)
        sd      s4, 4*8(tp)
        sd      s5, 5*8(tp)
        sd      s6, 6*8(tp)
        sd      s7, 7*8(tp)
        sd      s8, 8*8(tp)
        sd      s9, 9*8(tp)
        sd      s10, 10*8(tp)
        sd      s11, 11*8(tp)
        sd      ra, 12*8(tp)
        sd      sp, 13*8(tp)

        mv      tp, a0

        ld      sp, 13*8(tp)
        ld      ra, 12*8(tp)
        ld      s11, 11*8(tp)
        ld      s10, 10*8(tp)
        ld      s9, 9*8(tp)
        ld      s8, 8*8(tp)
        ld      s7, 7*8(tp)
        ld      s6, 6*8(tp)
        ld      s5, 5*8(tp)
        ld      s4, 4*8(tp)
        ld      s3, 3*8(tp)
        ld      s2, 2*8(tp)
        ld      s1, 1*8(tp)
        ld      s0, 0*8(tp)
                
        ret

        .global _thread_setup
        .type   _thread_setup, @function

# void _thread_setup (
#      struct thread * thr,             in a0
#      void * sp,                       in a1
#      void (*start)(void *, void *),   in a2
#      void * arg)                      in a3
#
# Sets up the initial context for a new thread. The thread will begin execution
# in /start/, receiving the five arguments passed to _thread_set after /start/.

_thread_setup:
        # Write initial register values into struct thread_context, which is the
        # first member of struct thread.
        
        sd      a1, 13*8(a0)    # Initial sp
        sd      a2, 11*8(a0)    # s11 <- start
        sd      a3, 0*8(a0)     # s0 <- arg 0
        sd      a4, 1*8(a0)     # s1 <- arg 1
        sd      a5, 2*8(a0)     # s2 <- arg 2
        sd      a6, 3*8(a0)     # s3 <- arg 3
        sd      a7, 4*8(a0)     # s4 <- arg 4

        # put address of thread entry glue into t1 and continue execution at 1f

        jal     t0, 1f

        # The glue code below is executed when we first switch into the new thread

        la      ra, thread_exit # child will return to thread_exit
        mv      a0, s0          # get arg argument to child from s0
        mv      a1, s1          # get arg argument to child from s0
        mv      fp, sp          # frame pointer = stack pointer
        jr      s11             # jump to child entry point (in s1)

1:      # Execution of _thread_setup continues here

        sd      t0, 12*8(a0)    # put address of above glue code into ra slot

        ret

        .global _thread_finish_jump
        .type   _thread_finish_jump, @function

# void __attribute__ ((noreturn)) _thread_finish_jump (
#      struct thread_stack_anchor * stack_anchor,
#      uintptr_t usp, uintptr_t upc, ...);

# Input: stack_anchor -- the struct thread_stack_anchor pointer located at the base of the stack
#        usp -- the stack pointer of the user mode 
#        upc -- the program entry of the user process
#
# Output: None.
#
# The function save the stack anchor pointer in register sscratch, and set the stvec register to User mode entry;
# It also sets the sepc to the entry of the program, so that the program can be executed in User mode after calling the "sret" instruction;

_thread_finish_jump:
        # While in user mode, sscratch points to a struct thread_stack_anchor
        # located at the base of the stack, which contains the current thread
        # pointer and serves as our starting stack pointer.

        # TODO: FIXME your code here

        # Set the register sscratch
        csrw sscratch, a0   

        # Set the register stvec to _trap_entry_from_umode
        la t0, _trap_entry_from_umode
        csrw stvec, t0        

        # Set the stack pointer to the user sp
        mv sp, a1 
        # Set the register sepc to the entry of the program                 
        csrw sepc, a2

        sret


# void _thread_finish_fork(struct thread * child, const struct trap_frame * parent_tfr)
#
# Sets up the initial context for a new thread created by fork. The child will
# begin execution in the same function as the parent, receiving the same
# arguments. The child will return from _thread_finish_fork to the parent
# function, which will return to the child function.

        .global _thread_finish_fork
        .type   _thread_finish_fork, @function

        .equ    SP_ANCHOR, 15
        .equ    MSTATUS_TRAP_FRAME, 32
        .equ    MEPC_TRAP_FRAME, 33

_thread_finish_fork:
        
        # Save the current running thread's context to its stack
        sd      s0, 0*8(tp)       
        sd      s1, 1*8(tp)        
        sd      s2, 2*8(tp)        
        sd      s3, 3*8(tp)        
        sd      s4, 4*8(tp)        
        sd      s5, 5*8(tp)        
        sd      s6, 6*8(tp)        
        sd      s7, 7*8(tp)        
        sd      s8, 8*8(tp)        
        sd      s9, 9*8(tp)        
        sd      s10, 10*8(tp)      
        sd      s11, 11*8(tp)      
        sd      ra, 12*8(tp)       
        sd      sp, 13*8(tp)       
        
        # Switch to the child thread's context
        mv      tp, a0       

        # Set trap vector for user mode
        la      a0, _trap_entry_from_umode  
        csrw    stvec, a0   

        # Load parent's trap frame (tfr)
        mv      x31, a1       

        # Load the child thread's stack anchor and set it as sscratch
        ld      sp, SP_ANCHOR*8(tp)  
        csrw    sscratch, sp  
        
        # Restore sstatus register from the parent trap frame
        ld      a0, MSTATUS_TRAP_FRAME*8(x31)  
        csrw    sstatus, a0                   
        
        # Restore sepc register from the parent trap frame
        ld      a0, MEPC_TRAP_FRAME*8(x31)    
        csrw    sepc, a0                            
        
        # Restore general-purpose registers from the parent's trap frame
        ld      x1, 1*8(x31)       
        nop                        # No operation (placeholder to avoid timing hazard)
        ld      x3, 3*8(x31)       
        ld      x4, 4*8(x31)       
        ld      x5, 5*8(x31)       
        ld      x6, 6*8(x31)       
        ld      x7, 7*8(x31)       
        ld      x8, 8*8(x31)       
        ld      x9, 9*8(x31)       
        ld      x10, 10*8(x31)     
        ld      x11, 11*8(x31)     
        ld      x12, 12*8(x31)     
        ld      x13, 13*8(x31)     
        ld      x14, 14*8(x31)     
        ld      x15, 15*8(x31)     
        ld      x16, 16*8(x31)     
        ld      x17, 17*8(x31)    
        ld      x18, 18*8(x31)     
        ld      x19, 19*8(x31)     
        ld      x20, 20*8(x31)     
        ld      x21, 21*8(x31)     
        ld      x22, 22*8(x31)     
        ld      x23, 23*8(x31)     
        ld      x24, 24*8(x31)     
        ld      x25, 25*8(x31)     
        ld      x26, 26*8(x31)     
        ld      x27, 27*8(x31)     
        ld      x28, 28*8(x31)     
        ld      x29, 29*8(x31)     
        ld      x30, 30*8(x31)     
        ld      x31, 31*8(x31)     
        
        # Set the return value to 0 for the child process
        li      a0, 0              
        
        # Perform a system return to user mode
        sret                       



# Statically allocated stack for the idle thread.

        .section        .data.stack, "wa", @progbits
        .balign          16
        
        .equ            IDLE_STACK_SIZE, 4096

        .global         _idle_stack_lowest
        .type           _idle_stack_lowest, @object
        .size           _idle_stack_lowest, IDLE_STACK_SIZE

        .global         _idle_stack_anchor
        .type           _idle_stack_anchor, @object
        .size           _idle_stack_anchor, 2*8

_idle_stack_lowest:
        .fill   IDLE_STACK_SIZE, 1, 0xA5

_idle_stack_anchor:
        .global idle_thread # from thread.c
        .dword  idle_thread
        .fill   8
        .end

        